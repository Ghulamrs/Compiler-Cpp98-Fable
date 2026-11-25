// Coff.cpp - version 5: MASM-subset assembler and COFF object writer (x86-64).
//
// Encoding notes (Intel SDM volume 2):
//   REX = 0100WRXB   W: 64-bit operand   R: ModRM.reg bit 3   X: SIB.index bit 3   B: ModRM.rm / SIB.base bit 3
//   ModRM = mod(2) reg(3) rm(3); rm=100 means a SIB byte follows; mod=00 rm=101 means RIP+disp32
//   SIB = scale(2) index(3) base(3); index=100 means no index; base=101 with mod=00 means disp32 and no base
//   mandatory prefixes 66/F2/F3 go before REX; 0F is the two-byte opcode escape
// Relocations: IMAGE_REL_AMD64_REL32 (+n when n immediate bytes follow the field), IMAGE_REL_AMD64_ADDR64,
// IMAGE_REL_AMD64_ADDR32NB (the function table in .pdata).
// Unwind info: a PROC FRAME's prologue directives become its UNWIND_INFO in .xdata and a RUNTIME_FUNCTION entry
// (begin, end, unwind info) in .pdata, as ml64 writes them; a COMDAT function's go into associative COMDAT sections.
// Debug info: the ";@debug" and ";@line" markers of the listing become CodeView line tables and procedure symbols in
// .debug$S sections (one for .text with the module's file tables, an associative one per COMDAT function), which
// link /DEBUG puts in the PDB (the records are cppparse/cv's, see "CodeView debug information" below).
//
// Strict C++98.
#include "Coff.hpp"
#include "CvCheck.hpp"
#include "CvModule.hpp"
#include "CvScopes.hpp"
#include "CvNames.hpp"
#include "CvSection.hpp"
#include "CvTypes.hpp"
#include "EhClFuncInfo.hpp"
#include "EhUnwindInfo.hpp"
#include "Parallel.hpp"
#include "TargetTls.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace cppparse {

namespace {


std::string trimWs(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lowerText(const std::string& s)
{
    std::string r(s);
    for (size_t i = 0; i < r.size(); ++i) r[i] = static_cast<char>(tolower(static_cast<unsigned char>(r[i])));
    return r;
}

std::string upperText(const std::string& s)
{
    std::string r(s);
    for (size_t i = 0; i < r.size(); ++i) r[i] = static_cast<char>(toupper(static_cast<unsigned char>(r[i])));
    return r;
}

std::string numText(long v) { std::ostringstream s; s << v; return s.str(); }

// a byte of 0x80 and above belongs to a name too: an extended character of an identifier, in UTF-8 as cl decorates it
bool isIdentStart(char c) { return isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '@' || c == '$' || c == '?' || static_cast<unsigned char>(c) >= 0x80; }
bool isIdentChar(char c) { return isIdentStart(c) || isdigit(static_cast<unsigned char>(c)); }

// splits operands on commas that are outside brackets
void splitOperands(const std::string& text, std::vector<std::string>& out)
{
    int depth = 0;
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '[') ++depth;
        else if (c == ']') --depth;
        if (c == ',' && depth == 0) { out.push_back(trimWs(cur)); cur.clear(); }
        else cur += c;
    }
    if (!trimWs(cur).empty()) out.push_back(trimWs(cur));
}

// unsigned 64-bit value kept in two halves (C++98 has no 64-bit integer type)
struct U64 {
    unsigned long lo, hi;
    U64() : lo(0), hi(0) {}
};

void u64MulAdd(U64& v, unsigned long mul, unsigned long add)   // v = v * mul + add, mul <= 16
{
    unsigned long limb[4] = { v.lo & 0xFFFFUL, (v.lo >> 16) & 0xFFFFUL, v.hi & 0xFFFFUL, (v.hi >> 16) & 0xFFFFUL };
    unsigned long carry = add;
    for (int i = 0; i < 4; ++i) {
        unsigned long t = limb[i] * mul + carry;
        limb[i] = t & 0xFFFFUL;
        carry = t >> 16;
    }
    v.lo = limb[0] | (limb[1] << 16);
    v.hi = limb[2] | (limb[3] << 16);
}

void u64Negate(U64& v)
{
    v.lo = ~v.lo; v.hi = ~v.hi;
    unsigned long lo = (v.lo + 1) & 0xFFFFFFFFUL;
    if (lo == 0) v.hi = (v.hi + 1) & 0xFFFFFFFFUL;
    v.lo = lo;
}

// decimal, or hexadecimal with a trailing 'h'
bool parseU64(const std::string& text, U64& out)
{
    std::string t = lowerText(trimWs(text));
    bool neg = false;
    if (!t.empty() && (t[0] == '-' || t[0] == '+')) { neg = t[0] == '-'; t = t.substr(1); }
    if (t.empty()) return false;
    U64 v;
    if (t[t.size() - 1] == 'h') {
        for (size_t i = 0; i + 1 < t.size(); ++i) {
            char c = t[i];
            unsigned long d;
            if (c >= '0' && c <= '9') d = static_cast<unsigned long>(c - '0');
            else if (c >= 'a' && c <= 'f') d = static_cast<unsigned long>(c - 'a' + 10);
            else return false;
            u64MulAdd(v, 16, d);
        }
    } else {
        for (size_t i = 0; i < t.size(); ++i) {
            if (!isdigit(static_cast<unsigned char>(t[i]))) return false;
            u64MulAdd(v, 10, static_cast<unsigned long>(t[i] - '0'));
        }
    }
    if (neg) u64Negate(v);
    out = v;
    return true;
}

bool parseLong(const std::string& text, long& out)
{
    U64 v;
    if (!parseU64(text, v)) return false;
    // accept values that fit in a signed or unsigned 32-bit field
    if (v.hi != 0 && v.hi != 0xFFFFFFFFUL) return false;
    out = static_cast<long>(static_cast<unsigned int>(v.lo));
    return true;
}

bool isNumberText(const std::string& t)
{
    U64 v;
    return !t.empty() && (isdigit(static_cast<unsigned char>(t[0])) || t[0] == '-' || t[0] == '+') && parseU64(t, v);
}


struct RegInfo { const char* name; int num; int size; };

const RegInfo kRegs[] = {
    {"rax",0,8},{"rcx",1,8},{"rdx",2,8},{"rbx",3,8},{"rsp",4,8},{"rbp",5,8},{"rsi",6,8},{"rdi",7,8},
    {"r8",8,8},{"r9",9,8},{"r10",10,8},{"r11",11,8},{"r12",12,8},{"r13",13,8},{"r14",14,8},{"r15",15,8},
    {"eax",0,4},{"ecx",1,4},{"edx",2,4},{"ebx",3,4},{"esp",4,4},{"ebp",5,4},{"esi",6,4},{"edi",7,4},
    {"r8d",8,4},{"r9d",9,4},{"r10d",10,4},{"r11d",11,4},{"r12d",12,4},{"r13d",13,4},{"r14d",14,4},{"r15d",15,4},
    {"ax",0,2},{"cx",1,2},{"dx",2,2},{"bx",3,2},{"sp",4,2},{"bp",5,2},{"si",6,2},{"di",7,2},
    {"r8w",8,2},{"r9w",9,2},{"r10w",10,2},{"r11w",11,2},{"r12w",12,2},{"r13w",13,2},{"r14w",14,2},{"r15w",15,2},
    {"al",0,1},{"cl",1,1},{"dl",2,1},{"bl",3,1},{"spl",4,1},{"bpl",5,1},{"sil",6,1},{"dil",7,1},
    {"r8b",8,1},{"r9b",9,1},{"r10b",10,1},{"r11b",11,1},{"r12b",12,1},{"r13b",13,1},{"r14b",14,1},{"r15b",15,1},
    {"r8l",8,1},{"r9l",9,1},{"r10l",10,1},{"r11l",11,1},{"r12l",12,1},{"r13l",13,1},{"r14l",14,1},{"r15l",15,1}
};

bool lookupReg(const std::string& lname, int& num, int& size)
{
    for (size_t i = 0; i < sizeof(kRegs) / sizeof(kRegs[0]); ++i)
        if (lname == kRegs[i].name) { num = kRegs[i].num; size = kRegs[i].size; return true; }
    return false;
}

struct Operand {
    enum Kind { None, Reg, Xmm, Imm, Mem, Sym };
    Kind        kind;
    int         reg;      // Reg/Xmm number 0..15
    int         size;     // Reg: 1/2/4/8, Xmm: 16, Mem: 1/2/4/8/16 or 0 when not given
    long        imm;
    int         base, index, scale;   // Mem; -1 when absent
    long        disp;
    std::string symbol;   // Mem: RIP-relative symbol; Sym: the name
    bool        gs;       // Mem: gs:[...], the thread's own block (the segment prefix 65h; thread-locals, cppparse/target)
    bool        secRel;   // Sym: SECTIONREL <name>, the symbol's offset in its section (a 32-bit SECREL relocation)
    Operand() : kind(None), reg(-1), size(0), imm(0), base(-1), index(-1), scale(1), disp(0), gs(false), secRel(false) {}
    bool isReg() const { return kind == Reg; }
    bool isMem() const { return kind == Mem; }
    bool isRm()  const { return kind == Reg || kind == Mem; }
};

bool parseMemory(const std::string& innerText, Operand& op, std::string& err)
{
    // pieces separated by + or -, each a register, register*scale, number or symbol
    std::string inner = trimWs(innerText);
    size_t i = 0;
    bool first = true;
    while (i < inner.size()) {
        int sign = 1;
        while (i < inner.size() && isspace(static_cast<unsigned char>(inner[i]))) ++i;
        if (i < inner.size() && (inner[i] == '+' || inner[i] == '-')) { sign = inner[i] == '-' ? -1 : 1; ++i; }
        else if (!first) { err = "bad memory operand [" + inner + "]"; return false; }
        while (i < inner.size() && isspace(static_cast<unsigned char>(inner[i]))) ++i;
        size_t s = i;
        while (i < inner.size() && inner[i] != '+' && inner[i] != '-') ++i;
        std::string piece = trimWs(inner.substr(s, i - s));
        first = false;
        if (piece.empty()) { err = "bad memory operand [" + inner + "]"; return false; }
        std::string lp = lowerText(piece);
        size_t star = lp.find('*');
        int num, size;
        if (star != std::string::npos) {
            std::string r = trimWs(lp.substr(0, star));
            long sc = 0;
            if (!lookupReg(r, num, size) || size != 8 || !parseLong(lp.substr(star + 1), sc) || (sc != 1 && sc != 2 && sc != 4 && sc != 8) || op.index >= 0) {
                err = "bad index in [" + inner + "]"; return false;
            }
            op.index = num; op.scale = static_cast<int>(sc);
        } else if (lookupReg(lp, num, size)) {
            if (size != 8) { err = "memory operand needs 64-bit registers: [" + inner + "]"; return false; }
            if (op.base < 0) op.base = num;
            else if (op.index < 0) { op.index = num; op.scale = 1; }
            else { err = "too many registers in [" + inner + "]"; return false; }
        } else if (isNumberText(lp)) {
            long v;
            if (!parseLong(lp, v)) { err = "bad displacement in [" + inner + "]"; return false; }
            op.disp += sign * v;
        } else if (isIdentStart(piece[0])) {
            if (!op.symbol.empty()) { err = "two symbols in [" + inner + "]"; return false; }
            op.symbol = piece;
        } else { err = "bad memory operand [" + inner + "]"; return false; }
    }
    if (!op.symbol.empty() && (op.base >= 0 || op.index >= 0)) { err = "symbol with registers is not RIP-addressable: [" + inner + "]"; return false; }
    if (op.index == 4) { err = "rsp cannot be an index register: [" + inner + "]"; return false; }
    return true;
}

bool parseOperand(const std::string& text, Operand& op, std::string& err)
{
    std::string t = trimWs(text);
    std::string l = lowerText(t);
    int num, size;
    if (lookupReg(l, num, size)) { op.kind = Operand::Reg; op.reg = num; op.size = size; return true; }
    if (l.size() >= 4 && l.compare(0, 3, "xmm") == 0 && isdigit(static_cast<unsigned char>(l[3]))) {
        long n = atol(l.c_str() + 3);
        if (n < 0 || n > 15) { err = "bad register " + t; return false; }
        op.kind = Operand::Xmm; op.reg = static_cast<int>(n); op.size = 16; return true;
    }
    if (isNumberText(l)) {
        if (!parseLong(l, op.imm)) { err = "immediate does not fit in 32 bits: " + t; return false; }
        op.kind = Operand::Imm; return true;
    }
    size_t br = l.find('[');
    if (br != std::string::npos) {
        if (l[l.size() - 1] != ']') { err = "bad operand " + t; return false; }
        std::string pre = trimWs(l.substr(0, br));
        op.kind = Operand::Mem;
        // "qword ptr gs:[58h]": the gs segment (only gs: a thread-local's access, TargetTls)
        if (pre.size() >= 3 && pre.compare(pre.size() - 3, 3, "gs:") == 0) { op.gs = true; pre = trimWs(pre.substr(0, pre.size() - 3)); }
        if (!pre.empty()) {
            if (pre == "byte ptr") op.size = 1;
            else if (pre == "word ptr") op.size = 2;
            else if (pre == "dword ptr" || pre == "real4 ptr") op.size = 4;
            else if (pre == "qword ptr" || pre == "real8 ptr") op.size = 8;
            else if (pre == "xmmword ptr") op.size = 16;
            else { err = "bad size prefix " + t; return false; }
        }
        return parseMemory(t.substr(br + 1, t.size() - br - 2), op, err);
    }
    if (l.compare(0, 7, "offset ") == 0) t = trimWs(t.substr(7));
    else if (l.compare(0, 11, "sectionrel ") == 0) { t = trimWs(t.substr(11)); op.secRel = true; }
    if (!t.empty() && isIdentStart(t[0])) {
        for (size_t i = 0; i < t.size(); ++i) if (!isIdentChar(t[i])) { err = "bad operand " + t; return false; }
        op.kind = Operand::Sym; op.symbol = t; return true;
    }
    err = "bad operand " + t;
    return false;
}


enum { RelAddr64 = 0x0001, RelAddr32Nb = 0x0003, RelRel32 = 0x0004, RelSecRel32 = 0x000B };   // SECREL: a thread-local's offset   // ADDR32NB: image-relative (.pdata entries)

struct Reloc {
    unsigned long  offset;    // within the section (chunk-relative until layout)
    std::string    symbol;
    unsigned short type;
};

struct Fixup {                // rel32 to a label of the same function
    unsigned long at;         // offset of the displacement field (4 bytes, or 1 byte once shortened)
    std::string   label;
    int           line;
    bool          jump;       // jmp/jcc (may be shortened) rather than call
    bool          cond;       // jcc
    bool          shortForm;
    unsigned long opcodeAt;   // offset of the first opcode byte (jumps only)
};

typedef EhUnwindStep UnwindStep;   // one prologue step of a PROC FRAME, from its unwind directive (cppparse/eh)

// the FuncInfo of a PROC in cl's exception model (--eh=cl), as the listing's ";@eh" lines give it: labels, to be
// resolved once the code is encoded (cppparse/eh EhClFuncInfo builds the tables)
struct ClCatchLine { unsigned long adjectives; std::string type; long dispObject; std::string handler; long dispFrame; };
struct ClTryLine { long low, high, catchHigh; std::vector<ClCatchLine> catches; };
struct ClLines {
    bool on;
    long unwindHelp;
    std::vector<std::pair<long, std::string> > states;       // the state it unwinds to, its action's label ("0": none)
    std::vector<ClTryLine> tries;
    std::vector<std::pair<std::string, long> > ips;          // a label and the state of the code from it on
    ClLines() : on(false), unwindHelp(0) {}
};

struct Chunk {
    std::vector<unsigned char>                bytes;
    std::vector<UnwindStep>                   unwind;     // .pushreg / .allocstack / .savexmm128, in prologue order
    long                                      prologEnd;  // .endprolog: code offset, -1 when absent
    std::vector<EhClFunclet>                  funclets;   // .funclet (cl's exception model): in code order
    ClLines                                   cl;         // .ehcl and the lines after it
    std::vector<unsigned long>                callEnds;   // the code offsets just behind every call (for EhClCheck)
    std::vector<std::vector<std::string> >    ehRanges;   // .ehrange begin, end, landing pad (labels of this function)
    std::vector<Reloc>                        relocs;
    std::vector<Fixup>                        fixups;
    std::map<std::string, unsigned long>      labels;    // local labels
    std::vector<std::pair<std::string, unsigned long> > symbols;   // named definitions (PROC names, data labels)
    std::vector<unsigned long>                padStarts;  // where ALIGN padding begins: a data item ends there
    unsigned long                             maxAlign;   // the largest ALIGN (or COMDAT marker alignment): the section's alignment
    std::vector<std::pair<unsigned long, int> > lines;    // .line (the listing's ;@line markers): code offset, source line, in offset order
    std::vector<std::pair<unsigned long, int> > cvMarks;  // .cvmark (-g: the listing's position markers): code offset, the marker's number
    std::vector<unsigned long>                retEnds;    // the code offsets just behind every ret (-g: CvCheck's epilogues)
    std::vector<std::string>                  errors;
    int                                       instructions;
    Chunk() : prologEnd(-1), maxAlign(1), instructions(0) {}
};


class Encoder {
public:
    explicit Encoder(Chunk& c) : m_c(c), m_pendingLine(-1) {}

    bool codeLine(const std::string& text, int line);
    bool dataLine(const std::string& text, int line);
    void finish();   // resolves the local fixups; unresolved labels become REL32 relocations

private:
    unsigned long pos() const { return static_cast<unsigned long>(m_c.bytes.size()); }
    void byte(int b) { m_c.bytes.push_back(static_cast<unsigned char>(b & 0xFF)); }
    void word(long v) { byte(static_cast<int>(v)); byte(static_cast<int>(v >> 8)); }
    void dword(long v) { for (int i = 0; i < 4; ++i) byte(static_cast<int>(v >> (8 * i))); }
    void qword(const U64& v) { dword(static_cast<long>(v.lo)); dword(static_cast<long>(v.hi)); }
    void imm(long v, int bytes) { for (int i = 0; i < bytes; ++i) byte(static_cast<int>(v >> (8 * i))); }
    // "line N in <function or data item>: <what> [<the line>]": the chunk's latest symbol is the PROC being encoded (a
    // code chunk holds its PROC name alone) or the data item the line belongs to; the driver reports the error as
    // one of the code generator, so the message carries what a report needs
    bool fail(const std::string& what, int line)
    {
        const std::string where = m_c.symbols.empty() ? std::string() : " in " + m_c.symbols.back().first;
        m_c.errors.push_back("line " + numText(line) + where + ": " + what + (m_line.empty() ? std::string() : " [" + m_line + "]"));
        return false;
    }

    bool instruction(const std::string& mn, std::vector<Operand>& ops, int line);
    bool unwindDirective(const std::string& mn, const std::vector<std::string>& opText, int line);
    void emitRm(int prefix, const unsigned char* op, int opLen, int regField, const Operand& rm, bool w, int trailing, bool forceRex);
    void rel32(const std::string& label, int line);
    void jump(int opcode, const std::string& label, int line);   // long form now, shortened in finish() when possible
    void shrinkJumps();
    static bool fitsImm8(long v) { return v >= -128 && v <= 127; }
    static bool byteRegsNeedRex(const std::vector<Operand>& ops)
    {
        for (size_t i = 0; i < ops.size(); ++i) if (ops[i].kind == Operand::Reg && ops[i].size == 1 && ops[i].reg >= 4 && ops[i].reg <= 7) return true;
        return false;
    }
    static int condCode(const std::string& cc);

    Chunk&      m_c;
    std::string m_line;   // the assembly line being encoded, quoted by the error messages
    int         m_pendingLine;   // a statement's .line at the function's start, recorded after its first instruction (-1: none)
};

int Encoder::condCode(const std::string& cc)
{
    if (cc == "o") return 0;  if (cc == "no") return 1;
    if (cc == "b" || cc == "c" || cc == "nae") return 2;  if (cc == "ae" || cc == "nb" || cc == "nc") return 3;
    if (cc == "e" || cc == "z") return 4;  if (cc == "ne" || cc == "nz") return 5;
    if (cc == "be" || cc == "na") return 6;  if (cc == "a" || cc == "nbe") return 7;
    if (cc == "s") return 8;  if (cc == "ns") return 9;
    if (cc == "p" || cc == "pe") return 10;  if (cc == "np" || cc == "po") return 11;
    if (cc == "l" || cc == "nge") return 12;  if (cc == "ge" || cc == "nl") return 13;
    if (cc == "le" || cc == "ng") return 14;  if (cc == "g" || cc == "nle") return 15;
    return -1;
}

// prefix (0, 0x66, 0xF2, 0xF3), opcode bytes, ModRM.reg field, the r/m operand, REX.W, bytes of immediate that
// follow (for the relocation type of RIP-relative operands), REX forced for spl/bpl/sil/dil
void Encoder::emitRm(int prefix, const unsigned char* op, int opLen, int regField, const Operand& rm, bool w, int trailing, bool forceRex)
{
    int rex = 0;
    if (w) rex |= 8;
    if (regField >= 8) rex |= 4;
    if (rm.kind == Operand::Reg || rm.kind == Operand::Xmm) { if (rm.reg >= 8) rex |= 1; }
    else { if (rm.index >= 8) rex |= 2; if (rm.base >= 8) rex |= 1; }
    if (prefix) byte(prefix);
    if (rex || forceRex) byte(0x40 | rex);
    for (int i = 0; i < opLen; ++i) byte(op[i]);
    int r = regField & 7;
    if (rm.kind == Operand::Reg || rm.kind == Operand::Xmm) { byte(0xC0 | (r << 3) | (rm.reg & 7)); return; }

    if (!rm.symbol.empty()) {                       // RIP-relative
        byte((r << 3) | 5);
        Reloc rl; rl.offset = pos(); rl.symbol = rm.symbol; rl.type = static_cast<unsigned short>(RelRel32 + trailing);
        m_c.relocs.push_back(rl);
        dword(rm.disp);
        return;
    }
    int scaleBits = rm.scale == 8 ? 3 : rm.scale == 4 ? 2 : rm.scale == 2 ? 1 : 0;
    if (rm.base < 0 && rm.index < 0) {               // absolute disp32 (SIB with no base and no index)
        byte((r << 3) | 4); byte(0x25); dword(rm.disp); return;
    }
    if (rm.base < 0) {                               // index only: mod=00, base=101 means disp32 without base
        byte((r << 3) | 4); byte((scaleBits << 6) | ((rm.index & 7) << 3) | 5); dword(rm.disp); return;
    }
    int baseLow = rm.base & 7;
    bool needSib = rm.index >= 0 || baseLow == 4;    // rsp/r12 as base always need a SIB
    int mod;
    if (rm.disp == 0 && baseLow != 5) mod = 0;       // rbp/r13 need a displacement
    else if (fitsImm8(rm.disp)) mod = 1;
    else mod = 2;
    if (needSib) {
        byte((mod << 6) | (r << 3) | 4);
        byte((scaleBits << 6) | ((rm.index < 0 ? 4 : (rm.index & 7)) << 3) | baseLow);
    } else {
        byte((mod << 6) | (r << 3) | baseLow);
    }
    if (mod == 1) byte(static_cast<int>(rm.disp));
    else if (mod == 2) dword(rm.disp);
}

void Encoder::rel32(const std::string& label, int line)
{
    Fixup f; f.at = pos(); f.label = label; f.line = line; f.jump = false; f.cond = false; f.shortForm = false; f.opcodeAt = pos();
    m_c.fixups.push_back(f);
    dword(0);
}

// opcode: 0xE9 for jmp, 0x80+cc for jcc (emitted as 0F 8x)
void Encoder::jump(int opcode, const std::string& label, int line)
{
    Fixup f; f.opcodeAt = pos(); f.label = label; f.line = line; f.jump = true; f.cond = opcode != 0xE9; f.shortForm = false;
    if (f.cond) { byte(0x0F); byte(opcode); } else byte(0xE9);
    f.at = pos();
    m_c.fixups.push_back(f);
    dword(0);
}

// turns long jumps whose target is within -128..127 bytes into the 2-byte form (like ml64); shrinking only
// ever brings targets closer, so repeating until nothing changes converges
void Encoder::shrinkJumps()
{
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < m_c.fixups.size(); ++i) {
            Fixup& f = m_c.fixups[i];
            if (!f.jump || f.shortForm) continue;
            std::map<std::string, unsigned long>::const_iterator it = m_c.labels.find(f.label);
            if (it == m_c.labels.end()) continue;
            unsigned long end = f.at + 4;                       // end of the long instruction
            unsigned long removed = end - (f.opcodeAt + 2);     // 3 for jmp, 4 for jcc
            unsigned long target = it->second;
            unsigned long newTarget = target >= end ? target - removed : target;
            long rel = static_cast<long>(newTarget) - static_cast<long>(f.opcodeAt + 2);
            if (rel < -128 || rel > 127) continue;
            // rewrite the instruction and close the gap
            m_c.bytes[f.opcodeAt] = static_cast<unsigned char>(f.cond ? 0x70 + (m_c.bytes[f.opcodeAt + 1] - 0x80) : 0xEB);
            m_c.bytes[f.opcodeAt + 1] = 0;
            m_c.bytes.erase(m_c.bytes.begin() + static_cast<long>(f.opcodeAt + 2), m_c.bytes.begin() + static_cast<long>(end));
            for (std::map<std::string, unsigned long>::iterator l = m_c.labels.begin(); l != m_c.labels.end(); ++l)
                if (l->second >= end) l->second -= removed;
            for (size_t k = 0; k < m_c.fixups.size(); ++k) {
                if (k == i) continue;
                if (m_c.fixups[k].opcodeAt >= end) { m_c.fixups[k].opcodeAt -= removed; m_c.fixups[k].at -= removed; }
            }
            for (size_t r = 0; r < m_c.relocs.size(); ++r) if (m_c.relocs[r].offset >= end) m_c.relocs[r].offset -= removed;
            for (size_t u = 0; u < m_c.unwind.size(); ++u) if (m_c.unwind[u].offset >= end) m_c.unwind[u].offset -= removed;
            if (m_c.prologEnd >= static_cast<long>(end)) m_c.prologEnd -= static_cast<long>(removed);
            for (size_t fn = 0; fn < m_c.funclets.size(); ++fn) {
                EhClFunclet& fc = m_c.funclets[fn];
                if (fc.start >= end) fc.start -= removed;
                for (size_t u = 0; u < fc.unwind.size(); ++u) if (fc.unwind[u].offset >= end) fc.unwind[u].offset -= removed;
                if (fc.prologEnd >= static_cast<long>(end)) fc.prologEnd -= static_cast<long>(removed);
            }
            for (size_t ce = 0; ce < m_c.callEnds.size(); ++ce) if (m_c.callEnds[ce] >= end) m_c.callEnds[ce] -= removed;
            for (size_t s = 0; s < m_c.symbols.size(); ++s) if (m_c.symbols[s].second >= end) m_c.symbols[s].second -= removed;
            for (size_t l = 0; l < m_c.lines.size(); ++l) if (m_c.lines[l].first >= end) m_c.lines[l].first -= removed;   // the line records move with the code
            for (size_t k = 0; k < m_c.cvMarks.size(); ++k) if (m_c.cvMarks[k].first >= end) m_c.cvMarks[k].first -= removed;
            for (size_t k = 0; k < m_c.retEnds.size(); ++k) if (m_c.retEnds[k] >= end) m_c.retEnds[k] -= removed;
            f.at = f.opcodeAt + 1;
            f.shortForm = true;
            changed = true;
        }
    }
}

void Encoder::finish()
{
    shrinkJumps();
    // a RIP-relative operand that names a label of this PROC (cl's exception model: "lea rax, <label>", the address a
    // handler funclet returns to go on at) is settled here; one that names a symbol stays a relocation
    for (size_t r = 0; r < m_c.relocs.size();) {
        const Reloc& rl = m_c.relocs[r];
        std::map<std::string, unsigned long>::const_iterator it = rl.type >= RelRel32 && rl.type <= RelRel32 + 5 ? m_c.labels.find(rl.symbol) : m_c.labels.end();
        if (it == m_c.labels.end()) { ++r; continue; }
        unsigned long rel = it->second - (rl.offset + 4 + static_cast<unsigned long>(rl.type - RelRel32));   // modulo 2^32, as the field
        for (int k = 0; k < 4; ++k) rel += static_cast<unsigned long>(m_c.bytes[rl.offset + static_cast<unsigned long>(k)]) << (8 * k);
        for (int k = 0; k < 4; ++k) m_c.bytes[rl.offset + static_cast<unsigned long>(k)] = static_cast<unsigned char>((rel >> (8 * k)) & 0xFF);
        m_c.relocs.erase(m_c.relocs.begin() + static_cast<std::ptrdiff_t>(r));
    }
    for (size_t i = 0; i < m_c.fixups.size(); ++i) {
        const Fixup& f = m_c.fixups[i];
        std::map<std::string, unsigned long>::const_iterator it = m_c.labels.find(f.label);
        if (it != m_c.labels.end()) {
            if (f.shortForm) {
                long rel = static_cast<long>(it->second) - static_cast<long>(f.at + 1);
                m_c.bytes[f.at] = static_cast<unsigned char>(rel & 0xFF);
            } else {
                long rel = static_cast<long>(it->second) - static_cast<long>(f.at + 4);
                for (int k = 0; k < 4; ++k) m_c.bytes[f.at + k] = static_cast<unsigned char>((rel >> (8 * k)) & 0xFF);
            }
        } else {                                      // a function or other external: let the linker resolve it
            Reloc rl; rl.offset = f.at; rl.symbol = f.label; rl.type = RelRel32;
            m_c.relocs.push_back(rl);
        }
    }
    m_c.fixups.clear();
}

bool Encoder::codeLine(const std::string& raw, int line)
{
    std::string text = trimWs(raw);
    if (text.empty()) return true;
    m_line = text;
    if (text[text.size() - 1] == ':' && text.find(' ') == std::string::npos) {   // label
        std::string name = text.substr(0, text.size() - 1);
        if (m_c.labels.count(name)) return fail("duplicate label " + name, line);
        m_c.labels[name] = pos();
        return true;
    }
    size_t sp = text.find_first_of(" \t");
    std::string mn = lowerText(sp == std::string::npos ? text : text.substr(0, sp));
    std::vector<std::string> opText;
    if (sp != std::string::npos) splitOperands(text.substr(sp + 1), opText);
    if (mn[0] == '.') return unwindDirective(mn, opText, line);
    // "lock <instruction>": the prefix F0h before it (the interlocked intrinsics, cppparse/target TargetIntrinsics)
    bool lockPrefix = false;
    if (mn == "lock" && sp != std::string::npos) {
        const std::string rest = trimWs(text.substr(sp + 1));
        const size_t sp2 = rest.find_first_of(" \t");
        mn = lowerText(sp2 == std::string::npos ? rest : rest.substr(0, sp2));
        opText.clear();
        if (sp2 != std::string::npos) splitOperands(rest.substr(sp2 + 1), opText);
        if (mn != "xadd" && mn != "cmpxchg" && mn != "xchg" && mn != "add" && mn != "sub" && mn != "inc" && mn != "dec") return fail("lock: not a read-modify-write instruction", line);
        lockPrefix = true;
    }
    std::vector<Operand> ops(opText.size());
    for (size_t i = 0; i < opText.size(); ++i) {
        std::string err;
        if (!parseOperand(opText[i], ops[i], err)) return fail(err, line);
    }
    size_t before = m_c.bytes.size();
    if (lockPrefix) byte(0xF0);
    if (!instruction(mn, ops, line)) { m_c.bytes.resize(before); return false; }
    if (mn == "call") m_c.callEnds.push_back(pos());
    else if (mn == "ret") m_c.retEnds.push_back(pos());
    ++m_c.instructions;
    if (m_pendingLine >= 0) { m_c.lines.push_back(std::make_pair(pos(), m_pendingLine)); m_pendingLine = -1; }
    return true;
}

// the MASM prologue directives of a PROC FRAME (the subset X86.cpp writes): each one describes the instruction
// just before it, so it records the current code offset
bool Encoder::unwindDirective(const std::string& mn, const std::vector<std::string>& opText, int line)
{
    if (mn == ".ehrange") {
        // a code range of a try region and its landing pad, for the language-specific data after the handler
        if (opText.size() != 3) return fail(".ehrange needs a begin label, an end label and a landing pad", line);
        m_c.ehRanges.push_back(opText);
        return true;
    }
    if (mn == ".line") {
        // a ;@line marker of the listing: the source line of the code from this offset on (the last one at an offset
        // wins). The function's own line (the first marker, at offset 0) keeps the first instruction when no prologue
        // separates it from the first statement's marker, as cl's tables start a function at its own line: that
        // statement's line is recorded after the first instruction instead
        long n = 0;
        if (opText.size() != 1 || !parseLong(opText[0], n) || n < 0) return fail(".line needs a line number", line);
        if (!m_c.lines.empty() && m_c.lines.back().first == pos()) {
            if (m_c.lines.size() == 1 && pos() == 0) m_pendingLine = static_cast<int>(n); else m_c.lines.back().second = static_cast<int>(n);
        }
        else { m_c.lines.push_back(std::make_pair(pos(), static_cast<int>(n))); m_pendingLine = -1; }
        return true;
    }
    if (mn == ".cvmark") {
        // -g: a position marker of the listing (;@prologend, ;@epilog, ;@scope): the code offset it stands at
        long k = 0;
        if (opText.size() != 1 || !parseLong(opText[0], k) || k < 0) return fail(".cvmark needs the marker's number", line);
        m_c.cvMarks.push_back(std::make_pair(pos(), static_cast<int>(k)));
        return true;
    }
    if (mn == ".funclet") {
        // a funclet of cl's exception model starts here: ".funclet 0|1|2" (a jump to a destructor, a cleanup with a
        // frame, a handler)
        long kind = 0;
        if (opText.size() != 1 || !parseLong(opText[0], kind) || kind < 0 || kind > 2) return fail(".funclet needs its kind, 0, 1 or 2", line);
        EhClFunclet fc;
        fc.start = pos();
        fc.kind = kind == 0 ? EhClFunclet::Jump : kind == 1 ? EhClFunclet::Cleanup : EhClFunclet::Catch;
        m_c.funclets.push_back(fc);
        return true;
    }
    if (mn == ".ehcl") {
        // the PROC's FuncInfo follows: ".ehcl <offset of the unwind-help slot from the establisher frame>"
        if (opText.size() != 1 || !parseLong(opText[0], m_c.cl.unwindHelp)) return fail(".ehcl needs the unwind-help offset", line);
        m_c.cl.on = true;
        return true;
    }
    if (mn == ".ehstate") {
        long to = 0;
        if (opText.size() != 2 || !parseLong(opText[0], to)) return fail(".ehstate needs the state it unwinds to and its action", line);
        m_c.cl.states.push_back(std::make_pair(to, opText[1]));
        return true;
    }
    if (mn == ".ehtry") {
        ClTryLine t;
        if (opText.size() != 3 || !parseLong(opText[0], t.low) || !parseLong(opText[1], t.high) || !parseLong(opText[2], t.catchHigh)) return fail(".ehtry needs tryLow, tryHigh, catchHigh", line);
        m_c.cl.tries.push_back(t);
        return true;
    }
    if (mn == ".ehcatch") {
        ClCatchLine h;
        long adjectives = 0;
        if (opText.size() != 5 || m_c.cl.tries.empty() || !parseLong(opText[0], adjectives) || !parseLong(opText[2], h.dispObject) || !parseLong(opText[4], h.dispFrame))
            return fail(".ehcatch needs adjectives, type descriptor, object offset, handler, frame offset, after an .ehtry", line);
        h.adjectives = static_cast<unsigned long>(adjectives); h.type = opText[1]; h.handler = opText[3];
        m_c.cl.tries.back().catches.push_back(h);
        return true;
    }
    if (mn == ".ehip") {
        long state = 0;
        if (opText.size() != 2 || !parseLong(opText[1], state)) return fail(".ehip needs a label and a state", line);
        m_c.cl.ips.push_back(std::make_pair(opText[0], state));
        return true;
    }
    // the prologue directives describe the PROC's own prologue, or the one of the funclet that has begun
    EhClFunclet* fc = m_c.funclets.empty() ? 0 : &m_c.funclets.back();
    if ((fc ? fc->prologEnd : m_c.prologEnd) >= 0) return fail(mn + " after .endprolog", line);
    if (mn == ".endprolog") {
        if (!opText.empty()) return fail(".endprolog takes no operands", line);
        (fc ? fc->prologEnd : m_c.prologEnd) = static_cast<long>(pos());
        return true;
    }
    UnwindStep s;
    s.offset = pos();
    s.reg = 0;
    s.value = 0;
    int num = 0, size = 0;
    long v = 0;
    if (mn == ".pushreg") {
        if (opText.size() != 1 || !lookupReg(lowerText(opText[0]), num, size) || size != 8) return fail(".pushreg needs a 64-bit register", line);
        s.op = UnwindStep::Push; s.reg = num;
    } else if (mn == ".allocstack") {
        if (opText.size() != 1 || !parseLong(opText[0], v) || v <= 0 || v % 8) return fail(".allocstack needs a positive multiple of 8", line);
        s.op = UnwindStep::Alloc; s.value = static_cast<unsigned long>(v);
    } else if (mn == ".savexmm128") {
        Operand x;
        std::string err;
        if (opText.size() != 2 || !parseOperand(opText[0], x, err) || x.kind != Operand::Xmm || !parseLong(opText[1], v) || v < 0 || v % 16)
            return fail(".savexmm128 needs an xmm register and an offset that is a multiple of 16", line);
        s.op = UnwindStep::SaveXmm; s.reg = x.reg; s.value = static_cast<unsigned long>(v);
    } else return fail("unsupported directive " + mn, line);
    (fc ? fc->unwind : m_c.unwind).push_back(s);
    return true;
}

// the unwind info of a PROC FRAME, built by cppparse/eh: its UNWIND_INFO (EhUnwindInfo) and, with a handler (PROC
// FRAME:handler, the table model), the handler's address and the range table of its ";@ehrange" lines (EhRangeTable).
// Every address is image-relative (ADDR32NB); 'relocs' gets them, at offsets within 'out', against 'fn' or the handler
bool buildUnwindInfo(const Chunk& c, const std::string& fn, const std::string& handler, std::vector<unsigned char>& out,
                     std::vector<Reloc>& relocs, std::string& err)
{
    if (!c.funclets.empty()) { err = ".funclet in a PROC that is not of cl's exception model (.ehcl)"; return false; }
    std::vector<unsigned char> info;
    if (!EhUnwindInfo::write(c.unwind, c.prologEnd, 0, handler.empty() ? 0 : EhUnwindInfo::FlagEHandler, info, err)) return false;
    out = info;
    relocs.clear();
    if (handler.empty()) {
        if (!c.ehRanges.empty()) { err = ".ehrange in a PROC FRAME without a handler"; return false; }
        return true;
    }
    std::vector<EhRange> ranges;
    for (size_t r = 0; r < c.ehRanges.size(); ++r) {
        unsigned long v[3] = { 0, 0, 0 };
        for (int k = 0; k < 3; ++k) {
            std::map<std::string, unsigned long>::const_iterator l = c.labels.find(c.ehRanges[r][static_cast<size_t>(k)]);
            if (l == c.labels.end()) { err = ".ehrange label " + c.ehRanges[r][static_cast<size_t>(k)] + " is not defined"; return false; }
            v[k] = l->second;
        }
        EhRange range; range.begin = v[0]; range.end = v[1]; range.pad = v[2];
        ranges.push_back(range);
    }
    std::vector<EhReloc> tableRelocs;
    if (!EhRangeTable::write(fn, handler, ranges, out, tableRelocs, err)) return false;
    for (size_t k = 0; k < tableRelocs.size(); ++k) { Reloc rl; rl.offset = tableRelocs[k].offset; rl.symbol = tableRelocs[k].symbol; rl.type = RelAddr32Nb; relocs.push_back(rl); }
    return true;
}

// cl's exception model: the PROC's ";@eh" lines with their labels resolved, checked (EhClCheck: what the runtime takes
// for granted) and built into the unwind info, the funclets' unwind info and the FuncInfo with its maps (EhClFuncInfo);
// 'entries' gets the function table entries of the PROC and its funclets
bool buildClInfo(const Chunk& c, const std::string& fn, const std::string& handler, std::vector<unsigned char>& out,
                 std::vector<Reloc>& relocs, std::vector<EhFrameEntry>& entries, std::string& err)
{
    EhClFunction f;
    f.unwind = c.unwind; f.prologEnd = c.prologEnd; f.codeSize = static_cast<unsigned long>(c.bytes.size());
    f.unwindHelp = c.cl.unwindHelp; f.funclets = c.funclets; f.callEnds = c.callEnds;
    for (size_t s = 0; s < c.cl.states.size(); ++s) {
        EhClState st; st.to = c.cl.states[s].first; st.action = -1;
        if (c.cl.states[s].second != "0") {
            std::map<std::string, unsigned long>::const_iterator l = c.labels.find(c.cl.states[s].second);
            if (l == c.labels.end()) { err = ".ehstate action " + c.cl.states[s].second + " is not defined"; return false; }
            st.action = static_cast<long>(l->second);
        }
        f.states.push_back(st);
    }
    for (size_t t = 0; t < c.cl.tries.size(); ++t) {
        EhClTry tb; tb.low = c.cl.tries[t].low; tb.high = c.cl.tries[t].high; tb.catchHigh = c.cl.tries[t].catchHigh;
        for (size_t h = 0; h < c.cl.tries[t].catches.size(); ++h) {
            const ClCatchLine& cl = c.cl.tries[t].catches[h];
            std::map<std::string, unsigned long>::const_iterator l = c.labels.find(cl.handler);
            if (l == c.labels.end()) { err = ".ehcatch handler " + cl.handler + " is not defined"; return false; }
            EhClCatch ch; ch.adjectives = cl.adjectives; ch.type = cl.type == "0" ? std::string() : cl.type; ch.dispObject = cl.dispObject; ch.handler = l->second; ch.dispFrame = cl.dispFrame;
            tb.catches.push_back(ch);
        }
        f.tries.push_back(tb);
    }
    for (size_t k = 0; k < c.cl.ips.size(); ++k) {
        std::map<std::string, unsigned long>::const_iterator l = c.labels.find(c.cl.ips[k].first);
        if (l == c.labels.end()) { err = ".ehip label " + c.cl.ips[k].first + " is not defined"; return false; }
        EhClIp ip; ip.at = l->second; ip.state = c.cl.ips[k].second;
        f.ips.push_back(ip);
    }
    if (!EhClCheck::check(f, err)) { err = "exception tables (--eh=cl): " + err; return false; }
    std::vector<EhReloc> built;
    if (!EhClFuncInfo::write(f, fn, "$unwind$" + fn, handler, out, built, entries, err)) return false;
    relocs.clear();
    for (size_t k = 0; k < built.size(); ++k) { Reloc rl; rl.offset = built[k].offset; rl.symbol = built[k].symbol; rl.type = RelAddr32Nb; relocs.push_back(rl); }
    return true;
}

struct SseOp { const char* name; int prefix; int opcode; int form; };   // form: 0 xmm,xmm/mem  1 store mem,xmm  2 xmm,r/m int  3 r,xmm/mem

const SseOp kSse[] = {
    {"movsd",0xF2,0x10,0},{"movss",0xF3,0x10,0},{"movapd",0x66,0x28,0},{"movaps",0x00,0x28,0},{"movupd",0x66,0x10,0},{"movups",0x00,0x10,0},
    {"movdqu",0xF3,0x6F,0},{"movdqa",0x66,0x6F,0},
    {"addsd",0xF2,0x58,0},{"mulsd",0xF2,0x59,0},{"subsd",0xF2,0x5C,0},{"divsd",0xF2,0x5E,0},{"minsd",0xF2,0x5D,0},{"maxsd",0xF2,0x5F,0},{"sqrtsd",0xF2,0x51,0},
    {"addss",0xF3,0x58,0},{"mulss",0xF3,0x59,0},{"subss",0xF3,0x5C,0},{"divss",0xF3,0x5E,0},{"minss",0xF3,0x5D,0},{"maxss",0xF3,0x5F,0},{"sqrtss",0xF3,0x51,0},
    {"addpd",0x66,0x58,0},{"mulpd",0x66,0x59,0},{"subpd",0x66,0x5C,0},{"divpd",0x66,0x5E,0},
    {"xorpd",0x66,0x57,0},{"xorps",0x00,0x57,0},{"andpd",0x66,0x54,0},{"andps",0x00,0x54,0},{"andnpd",0x66,0x55,0},{"orpd",0x66,0x56,0},{"orps",0x00,0x56,0},
    {"pxor",0x66,0xEF,0},{"pand",0x66,0xDB,0},{"por",0x66,0xEB,0},{"paddq",0x66,0xD4,0},{"psubq",0x66,0xFB,0},
    {"ucomisd",0x66,0x2E,0},{"comisd",0x66,0x2F,0},{"ucomiss",0x00,0x2E,0},{"comiss",0x00,0x2F,0},
    {"cvtsd2ss",0xF2,0x5A,0},{"cvtss2sd",0xF3,0x5A,0},
    {"cvtsi2sd",0xF2,0x2A,2},{"cvtsi2ss",0xF3,0x2A,2},
    {"cvttsd2si",0xF2,0x2C,3},{"cvttss2si",0xF3,0x2C,3},{"cvtsd2si",0xF2,0x2D,3},{"cvtss2si",0xF3,0x2D,3}
};

bool Encoder::instruction(const std::string& mn, std::vector<Operand>& ops, int line)
{
    size_t n = ops.size();
    bool forceRex = byteRegsNeedRex(ops);
    unsigned char op[3];

    if (n == 0) {
        if (mn == "ret") { byte(0xC3); return true; }
        if (mn == "leave") { byte(0xC9); return true; }
        if (mn == "cqo") { byte(0x48); byte(0x99); return true; }
        if (mn == "cdq") { byte(0x99); return true; }
        if (mn == "cdqe") { byte(0x48); byte(0x98); return true; }
        if (mn == "cwde") { byte(0x98); return true; }
        if (mn == "nop") { byte(0x90); return true; }
        if (mn == "int3") { byte(0xCC); return true; }
        if (mn == "ud2") { byte(0x0F); byte(0x0B); return true; }
        return fail("unknown instruction " + mn, line);
    }
    if (mn == "int" && n == 1 && ops[0].kind == Operand::Imm && ops[0].imm == 3) { byte(0xCC); return true; }   // ml64's spelling
    const Operand& a = ops[0];
    const Operand* b = n > 1 ? &ops[1] : 0;
    for (size_t i = 0; i < n; ++i) if (ops[i].kind == Operand::Mem && ops[i].gs) { if (!ops[i].symbol.empty()) return fail("gs: with a symbol", line); byte(0x65); break; }
    bool mixed = false;
    for (size_t i = 0; i < n; ++i) if (ops[i].kind == Operand::Xmm) mixed = true;

    for (size_t k = 0; k < sizeof(kSse) / sizeof(kSse[0]); ++k) {
        if (mn != kSse[k].name) continue;
        if (n != 2) return fail(mn + " needs two operands", line);
        const SseOp& s = kSse[k];
        if (s.form == 0 || s.form == 1) {
            if (a.kind == Operand::Xmm && (b->kind == Operand::Xmm || b->isMem())) {
                op[0] = 0x0F; op[1] = static_cast<unsigned char>(s.opcode);
                emitRm(s.prefix, op, 2, a.reg, *b, false, 0, false);
                return true;
            }
            if (a.isMem() && b->kind == Operand::Xmm) {
                int store = s.opcode == 0x10 ? 0x11 : s.opcode == 0x28 ? 0x29 : s.opcode == 0x6F ? 0x7F : -1;
                if (store < 0) return fail(mn + " cannot store to memory", line);
                op[0] = 0x0F; op[1] = static_cast<unsigned char>(store);
                emitRm(s.prefix, op, 2, b->reg, a, false, 0, false);
                return true;
            }
            return fail(mn + ": operands must be xmm registers or memory (float/integer mix?)", line);
        }
        if (s.form == 2) {   // cvtsi2sd xmm, r/m32|64
            if (a.kind != Operand::Xmm || !b->isRm()) return fail(mn + ": needs xmm, r/m", line);
            op[0] = 0x0F; op[1] = static_cast<unsigned char>(s.opcode);
            emitRm(s.prefix, op, 2, a.reg, *b, b->size == 8, 0, false);
            return true;
        }
        // form 3: cvttsd2si r32|64, xmm/m
        if (!a.isReg() || !(b->kind == Operand::Xmm || b->isMem())) return fail(mn + ": needs r, xmm/m", line);
        op[0] = 0x0F; op[1] = static_cast<unsigned char>(s.opcode);
        emitRm(s.prefix, op, 2, a.reg, *b, a.size == 8, 0, false);
        return true;
    }
    if (mn == "movq" || mn == "movd") {
        if (n != 2) return fail(mn + " needs two operands", line);
        bool w = mn == "movq";
        op[0] = 0x0F;
        if (a.kind == Operand::Xmm && b->isRm()) { op[1] = 0x6E; emitRm(0x66, op, 2, a.reg, *b, w, 0, false); return true; }
        if (a.isRm() && b->kind == Operand::Xmm) { op[1] = 0x7E; emitRm(0x66, op, 2, b->reg, a, w, 0, false); return true; }
        return fail(mn + ": bad operands", line);
    }
    if (mixed) return fail(mn + ": float register used with an integer operand", line);

    if (mn == "mov") {
        if (n != 2) return fail("mov needs two operands", line);
        if (a.isReg() && b->isReg()) {
            if (a.size != b->size) return fail("mov: register sizes differ", line);
            op[0] = static_cast<unsigned char>(a.size == 1 ? 0x88 : 0x89);
            emitRm(a.size == 2 ? 0x66 : 0, op, 1, b->reg, a, a.size == 8, 0, forceRex);
            return true;
        }
        if (a.isReg() && b->kind == Operand::Sym && b->secRel) {
            // mov r32, SECTIONREL sym: the symbol's offset in its section as a 32-bit immediate (zero-extended)
            if (a.size != 4) return fail("SECTIONREL needs a 32-bit register", line);
            if (a.reg >= 8) byte(0x41);
            byte(0xB8 + (a.reg & 7));
            Reloc rl; rl.offset = pos(); rl.symbol = b->symbol; rl.type = RelSecRel32;
            m_c.relocs.push_back(rl);
            dword(0);
            return true;
        }
        if (a.isReg() && b->kind == Operand::Imm) {
            long v = b->imm;
            if (a.size == 8) { op[0] = 0xC7; emitRm(0, op, 1, 0, a, true, 4, false); dword(v); return true; }   // sign-extended imm32, as ml64
            if (a.size == 4) { if (a.reg >= 8) byte(0x41); byte(0xB8 + (a.reg & 7)); dword(v); return true; }
            if (a.size == 2) { byte(0x66); if (a.reg >= 8) byte(0x41); byte(0xB8 + (a.reg & 7)); word(v); return true; }
            if (a.reg >= 8 || forceRex) byte(0x40 | (a.reg >= 8 ? 1 : 0));
            byte(0xB0 + (a.reg & 7)); byte(static_cast<int>(v)); return true;
        }
        if (a.isReg() && b->isMem()) {
            if (b->size && b->size != a.size) return fail("mov: operand sizes differ", line);
            op[0] = static_cast<unsigned char>(a.size == 1 ? 0x8A : 0x8B);
            emitRm(a.size == 2 ? 0x66 : 0, op, 1, a.reg, *b, a.size == 8, 0, forceRex);
            return true;
        }
        if (a.isMem() && b->isReg()) {
            if (a.size && a.size != b->size) return fail("mov: operand sizes differ", line);
            op[0] = static_cast<unsigned char>(b->size == 1 ? 0x88 : 0x89);
            emitRm(b->size == 2 ? 0x66 : 0, op, 1, b->reg, a, b->size == 8, 0, forceRex);
            return true;
        }
        if (a.isMem() && b->kind == Operand::Imm) {
            int sz = a.size ? a.size : 8;
            op[0] = static_cast<unsigned char>(sz == 1 ? 0xC6 : 0xC7);
            int ib = sz == 1 ? 1 : sz == 2 ? 2 : 4;
            emitRm(sz == 2 ? 0x66 : 0, op, 1, 0, a, sz == 8, ib, false);
            imm(b->imm, ib);
            return true;
        }
        return fail("mov: bad operands", line);
    }

    if (mn == "lea") {
        if (n != 2 || !a.isReg()) return fail("lea: bad operands", line);
        Operand m = *b;
        if (m.kind == Operand::Sym) { m.kind = Operand::Mem; m.size = 0; }   // lea r, symbol  ->  [rip+symbol]
        if (!m.isMem()) return fail("lea: second operand must be memory", line);
        op[0] = 0x8D;
        emitRm(a.size == 2 ? 0x66 : 0, op, 1, a.reg, m, a.size == 8, 0, false);
        return true;
    }

    {
        static const char* const names[] = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };
        int digit = -1;
        for (int k = 0; k < 8; ++k) if (mn == names[k]) digit = k;
        if (digit >= 0) {
            if (n != 2) return fail(mn + " needs two operands", line);
            int sz = a.isReg() ? a.size : a.size ? a.size : (b->isReg() ? b->size : 8);
            int pre = sz == 2 ? 0x66 : 0;
            if (a.isRm() && b->isReg()) {
                if (a.isReg() && a.size != b->size) return fail(mn + ": register sizes differ", line);
                op[0] = static_cast<unsigned char>(digit * 8 + (sz == 1 ? 0 : 1));
                emitRm(pre, op, 1, b->reg, a, sz == 8, 0, forceRex);
                return true;
            }
            if (a.isReg() && b->isMem()) {
                op[0] = static_cast<unsigned char>(digit * 8 + (sz == 1 ? 2 : 3));
                emitRm(pre, op, 1, a.reg, *b, sz == 8, 0, forceRex);
                return true;
            }
            if (a.isRm() && b->kind == Operand::Imm) {
                if (sz == 1) { op[0] = 0x80; emitRm(0, op, 1, digit, a, false, 1, forceRex); byte(static_cast<int>(b->imm)); return true; }
                if (fitsImm8(b->imm)) { op[0] = 0x83; emitRm(pre, op, 1, digit, a, sz == 8, 1, false); byte(static_cast<int>(b->imm)); return true; }
                int ib = sz == 2 ? 2 : 4;
                op[0] = 0x81; emitRm(pre, op, 1, digit, a, sz == 8, ib, false); imm(b->imm, ib); return true;
            }
            return fail(mn + ": bad operands", line);
        }
    }

    if (mn == "test") {
        if (n != 2) return fail("test needs two operands", line);
        if (a.isRm() && b->isReg()) {
            int sz = b->size;
            op[0] = static_cast<unsigned char>(sz == 1 ? 0x84 : 0x85);
            emitRm(sz == 2 ? 0x66 : 0, op, 1, b->reg, a, sz == 8, 0, forceRex);
            return true;
        }
        if (a.isRm() && b->kind == Operand::Imm) {
            int sz = a.size ? a.size : 8;
            int ib = sz == 1 ? 1 : sz == 2 ? 2 : 4;
            op[0] = static_cast<unsigned char>(sz == 1 ? 0xF6 : 0xF7);
            emitRm(sz == 2 ? 0x66 : 0, op, 1, 0, a, sz == 8, ib, forceRex);
            imm(b->imm, ib);
            return true;
        }
        return fail("test: bad operands", line);
    }

    if (mn == "imul") {
        if (n == 2 && a.isReg() && b->isRm()) { op[0] = 0x0F; op[1] = 0xAF; emitRm(a.size == 2 ? 0x66 : 0, op, 2, a.reg, *b, a.size == 8, 0, false); return true; }
        if (n == 3 && a.isReg() && b->isRm() && ops[2].kind == Operand::Imm) {
            long v = ops[2].imm;
            if (fitsImm8(v)) { op[0] = 0x6B; emitRm(a.size == 2 ? 0x66 : 0, op, 1, a.reg, *b, a.size == 8, 1, false); byte(static_cast<int>(v)); return true; }
            int ib = a.size == 2 ? 2 : 4;
            op[0] = 0x69; emitRm(a.size == 2 ? 0x66 : 0, op, 1, a.reg, *b, a.size == 8, ib, false); imm(v, ib); return true;
        }
        if (n != 1) return fail("imul: bad operands", line);
        // one-operand form falls through to the F7 group
    }

    {
        static const char* const names[] = { "", "", "not", "neg", "mul", "imul", "div", "idiv" };
        int digit = -1;
        for (int k = 2; k < 8; ++k) if (mn == names[k]) digit = k;
        if (digit >= 0) {
            if (n != 1 || !a.isRm()) return fail(mn + ": needs one register or memory operand", line);
            int sz = a.size ? a.size : 8;
            op[0] = static_cast<unsigned char>(sz == 1 ? 0xF6 : 0xF7);
            emitRm(sz == 2 ? 0x66 : 0, op, 1, digit, a, sz == 8, 0, forceRex);
            return true;
        }
    }

    if (mn == "inc" || mn == "dec") {
        if (n != 1 || !a.isRm()) return fail(mn + ": bad operand", line);
        int sz = a.size ? a.size : 8;
        op[0] = static_cast<unsigned char>(sz == 1 ? 0xFE : 0xFF);
        emitRm(sz == 2 ? 0x66 : 0, op, 1, mn == "inc" ? 0 : 1, a, sz == 8, 0, forceRex);
        return true;
    }

    {
        static const char* const names[] = { "rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar" };
        int digit = -1;
        for (int k = 0; k < 8; ++k) if (mn == names[k]) digit = k == 6 ? 4 : k;
        if (digit >= 0) {
            if (n != 2 || !a.isRm()) return fail(mn + ": bad operands", line);
            int sz = a.size ? a.size : 8;
            int pre = sz == 2 ? 0x66 : 0;
            if (b->kind == Operand::Imm) {
                if (b->imm == 1) { op[0] = static_cast<unsigned char>(sz == 1 ? 0xD0 : 0xD1); emitRm(pre, op, 1, digit, a, sz == 8, 0, forceRex); return true; }
                op[0] = static_cast<unsigned char>(sz == 1 ? 0xC0 : 0xC1); emitRm(pre, op, 1, digit, a, sz == 8, 1, forceRex); byte(static_cast<int>(b->imm)); return true;
            }
            if (b->isReg() && b->reg == 1 && b->size == 1) { op[0] = static_cast<unsigned char>(sz == 1 ? 0xD2 : 0xD3); emitRm(pre, op, 1, digit, a, sz == 8, 0, forceRex); return true; }
            return fail(mn + ": count must be an immediate or cl", line);
        }
    }

    if (mn.size() > 3 && mn.compare(0, 3, "set") == 0) {
        int cc = condCode(mn.substr(3));
        if (cc < 0) return fail("unknown instruction " + mn, line);
        if (n != 1 || !a.isRm() || (a.isReg() && a.size != 1)) return fail(mn + ": needs an 8-bit operand", line);
        op[0] = 0x0F; op[1] = static_cast<unsigned char>(0x90 + cc);
        emitRm(0, op, 2, 0, a, false, 0, forceRex);
        return true;
    }

    if (mn == "movzx" || mn == "movsx" || mn == "movsxd") {
        if (n != 2 || !a.isReg() || !b->isRm()) return fail(mn + ": bad operands", line);
        int srcSize = b->isReg() ? b->size : b->size;
        if (mn == "movsxd") {
            if (srcSize && srcSize != 4) return fail("movsxd: source must be 32 bits", line);
            op[0] = 0x63; emitRm(0, op, 1, a.reg, *b, a.size == 8, 0, false); return true;
        }
        if (srcSize != 1 && srcSize != 2) return fail(mn + ": source must be 8 or 16 bits (give a size prefix)", line);
        op[0] = 0x0F; op[1] = static_cast<unsigned char>((mn == "movzx" ? 0xB6 : 0xBE) + (srcSize == 2 ? 1 : 0));
        emitRm(a.size == 2 ? 0x66 : 0, op, 2, a.reg, *b, a.size == 8, 0, forceRex);
        return true;
    }

    if (mn == "jmp") {
        if (n != 1) return fail("jmp: bad operand", line);
        if (a.kind == Operand::Sym) { jump(0xE9, a.symbol, line); return true; }
        if (a.isRm()) { op[0] = 0xFF; emitRm(0, op, 1, 4, a, false, 0, false); return true; }
        return fail("jmp: bad operand", line);
    }
    if (mn.size() > 1 && mn[0] == 'j' && condCode(mn.substr(1)) >= 0) {
        if (n != 1 || a.kind != Operand::Sym) return fail(mn + ": needs a label", line);
        jump(0x80 + condCode(mn.substr(1)), a.symbol, line); return true;
    }
    if (mn == "call") {
        if (n != 1) return fail("call: bad operand", line);
        if (a.kind == Operand::Sym) { byte(0xE8); rel32(a.symbol, line); return true; }
        if (a.isRm()) { op[0] = 0xFF; emitRm(0, op, 1, 2, a, false, 0, false); return true; }
        return fail("call: bad operand", line);
    }

    if (mn == "push" || mn == "pop") {
        if (n != 1) return fail(mn + ": bad operand", line);
        if (a.isReg() && a.size == 8) { if (a.reg >= 8) byte(0x41); byte((mn == "push" ? 0x50 : 0x58) + (a.reg & 7)); return true; }
        if (mn == "push" && a.kind == Operand::Imm) { byte(0x68); dword(a.imm); return true; }
        if (a.isMem()) { op[0] = static_cast<unsigned char>(mn == "push" ? 0xFF : 0x8F); emitRm(0, op, 1, mn == "push" ? 6 : 0, a, false, 0, false); return true; }
        return fail(mn + ": bad operand", line);
    }
    if ((mn == "xadd" || mn == "cmpxchg") && n == 2 && a.isRm() && b->isReg()) {
        // 0F C1 / 0F B1 /r (0F C0 / 0F B0 for bytes): r/m, r
        const int sz = b->size;
        op[0] = 0x0F; op[1] = static_cast<unsigned char>((mn == "xadd" ? 0xC0 : 0xB0) + (sz == 1 ? 0 : 1));
        emitRm(sz == 2 ? 0x66 : 0, op, 2, b->reg, a, sz == 8, 0, forceRex);
        return true;
    }
    if (mn == "xchg" && n == 2 && a.isMem() && b->isReg()) { op[0] = static_cast<unsigned char>(b->size == 1 ? 0x86 : 0x87); emitRm(b->size == 2 ? 0x66 : 0, op, 1, b->reg, a, b->size == 8, 0, forceRex); return true; }
    if (mn == "xchg" && n == 2 && a.isReg() && b->isReg()) { op[0] = 0x87; emitRm(a.size == 2 ? 0x66 : 0, op, 1, b->reg, a, a.size == 8, 0, forceRex); return true; }

    return fail("unknown instruction " + mn, line);
}

// data directives: [name] DB|DW|DD|DQ values  |  [name] REAL8|REAL4 value  |  name LABEL type  |  ALIGN n
bool Encoder::dataLine(const std::string& raw, int line)
{
    std::string text = trimWs(raw);
    if (text.empty()) return true;
    m_line = text;
    std::vector<std::string> words;
    {
        std::istringstream in(text);
        std::string w;
        while (in >> w) words.push_back(w);
    }
    static const char* const kDirs[] = { "DB", "DW", "DD", "DQ", "REAL4", "REAL8", "LABEL", "ALIGN" };
    size_t di = 0;
    std::string name;
    std::string u0 = upperText(words[0]);
    bool firstIsDir = false;
    for (int k = 0; k < 8; ++k) if (u0 == kDirs[k]) firstIsDir = true;
    if (!firstIsDir) {
        if (words.size() < 2) return fail("bad data line: " + text, line);
        name = words[0];
        di = 1;
    }
    std::string dir = upperText(words[di]);
    if (dir == "ALIGN") {
        long n = 0;
        if (di + 1 >= words.size() || !parseLong(words[di + 1], n) || n <= 0 || n > 8192 || (n & (n - 1)) != 0) return fail("bad ALIGN", line);
        if (static_cast<unsigned long>(n) > m_c.maxAlign) m_c.maxAlign = static_cast<unsigned long>(n);
        if (pos() % static_cast<unsigned long>(n)) m_c.padStarts.push_back(pos());
        while (pos() % static_cast<unsigned long>(n)) byte(0);
        if (!name.empty()) m_c.symbols.push_back(std::make_pair(name, pos()));
        return true;
    }
    if (!name.empty()) m_c.symbols.push_back(std::make_pair(name, pos()));
    if (dir == "LABEL") return true;
    // searched after the name: a decorated name may end in the directive's letters ("?p@@3QBQEBDB DB ...")
    size_t valuesAt = text.find(words[di], name.size()) + words[di].size();
    std::vector<std::string> values;
    splitOperands(text.substr(valuesAt), values);
    if (values.empty()) return fail("missing value after " + dir, line);
    int size = dir == "DB" ? 1 : dir == "DW" ? 2 : dir == "DD" || dir == "REAL4" ? 4 : 8;
    for (size_t v = 0; v < values.size(); ++v) {
        std::string val = values[v];
        std::string lv = lowerText(val);
        if (dir == "REAL8" || dir == "REAL4") {
            // MASM's encoded real: "03FD0000000000000r", the bits in hex (a folded constant, exactly)
            const size_t digits = dir == "REAL8" ? 16 : 8;
            if (lv.size() == digits + 2 && lv[0] == '0' && lv[lv.size() - 1] == 'r' && lv.find_first_not_of("0123456789abcdef", 1) == lv.size() - 1) {
                for (size_t i = 0; i < digits / 2; ++i)   // little-endian: the last pair of digits first
                    byte(static_cast<unsigned char>(std::strtoul(lv.substr(lv.size() - 3 - 2 * i, 2).c_str(), 0, 16)));
                continue;
            }
            double d = strtod(val.c_str(), 0);
            if (dir == "REAL8") { unsigned char buf[8]; std::memcpy(buf, &d, 8); for (int i = 0; i < 8; ++i) byte(buf[i]); }
            else { float f = static_cast<float>(d); unsigned char buf[4]; std::memcpy(buf, &f, 4); for (int i = 0; i < 4; ++i) byte(buf[i]); }
            continue;
        }
        if (val == "?") { for (int i = 0; i < size; ++i) byte(0); continue; }
        size_t dup = lv.find(" dup");
        if (dup != std::string::npos) {                      // "N DUP (v)"
            long count = 0;
            std::string inner = trimWs(val.substr(dup + 4));
            if (!parseLong(val.substr(0, dup), count) || count < 0 || inner.size() < 2 || inner[0] != '(' || inner[inner.size() - 1] != ')') return fail("bad DUP: " + val, line);
            U64 u;
            std::string v2 = trimWs(inner.substr(1, inner.size() - 2));
            if (v2 != "?" && !parseU64(v2, u)) return fail("bad DUP value: " + val, line);
            for (long k = 0; k < count; ++k) { if (size == 8) qword(u); else imm(static_cast<long>(u.lo), size); }
            continue;
        }
        if (lv.compare(0, 9, "imagerel ") == 0) {
            // an image-relative address (cl's RTTI descriptors reference each other so): a 32-bit ADDR32NB relocation
            if (size != 4) return fail("IMAGEREL needs DD: " + val, line);
            Reloc rl; rl.offset = pos(); rl.symbol = trimWs(val.substr(9)); rl.type = RelAddr32Nb;
            m_c.relocs.push_back(rl);
            for (int i = 0; i < 4; ++i) byte(0);
            continue;
        }
        if (lv.compare(0, 7, "offset ") == 0 || (!val.empty() && isIdentStart(val[0]) && !isNumberText(val))) {
            std::string sym = trimWs(lv.compare(0, 7, "offset ") == 0 ? val.substr(7) : val);
            if (size != 8) return fail("an address needs DQ: " + val, line);
            Reloc rl; rl.offset = pos(); rl.symbol = sym; rl.type = RelAddr64;
            m_c.relocs.push_back(rl);
            for (int i = 0; i < 8; ++i) byte(0);
            continue;
        }
        U64 u;
        if (!parseU64(val, u)) return fail("bad value " + val, line);
        if (size == 8) qword(u);
        else imm(static_cast<long>(u.lo), size);
    }
    return true;
}



struct CodeBlock {
    std::string name;
    std::vector<std::pair<int, std::string> > lines;
    Chunk chunk;
    bool  comdat;      // one COMDAT section of its own (inline functions, template instantiations)
    bool  frame;       // PROC FRAME: gets unwind info (.xdata) and a function table entry (.pdata)
    std::string handler;   // PROC FRAME:handler, empty without one
    std::string sourceFile, displayName;   // ;@debug: the function's source file and the name its CodeView symbol shows
    CvProcedureMarks debug;                // -g: ;@proc, ;@block, ;@local, ;@gone, ;@static (CvScopes)
    std::vector<std::string> positions;    // -g: the position markers (;@prologend, ;@epilog, ;@scope), by the number .cvmark gives
    std::vector<unsigned char> xdata;
    std::vector<Reloc> xdataRelocs;
    std::vector<EhFrameEntry> entries;   // cl's exception model: the function table entries of the PROC and its funclets (else one, the PROC)
    CodeBlock() : comdat(false), frame(false) {}
};

struct EncodeJob : public ParallelJob {
    std::vector<CodeBlock>* blocks;
    virtual void run(size_t i)
    {
        CodeBlock& b = (*blocks)[i];
        Encoder enc(b.chunk);
        b.chunk.symbols.push_back(std::make_pair(b.name, 0UL));
        for (size_t k = 0; k < b.lines.size(); ++k) enc.codeLine(b.lines[k].second, b.lines[k].first);
        enc.finish();
        std::string err;
        if (b.frame && b.chunk.cl.on) {
            if (b.handler.empty()) b.chunk.errors.push_back(b.name + ": .ehcl in a PROC FRAME without a handler");
            else if (!buildClInfo(b.chunk, b.name, b.handler, b.xdata, b.xdataRelocs, b.entries, err)) b.chunk.errors.push_back(b.name + ": " + err);
        }
        else if (b.frame && !buildUnwindInfo(b.chunk, b.name, b.handler, b.xdata, b.xdataRelocs, err)) b.chunk.errors.push_back(b.name + ": " + err);
        if (!b.frame && (b.chunk.prologEnd >= 0 || !b.chunk.unwind.empty() || !b.chunk.ehRanges.empty() || !b.chunk.funclets.empty() || b.chunk.cl.on)) b.chunk.errors.push_back(b.name + ": unwind directives in a PROC without FRAME");
    }
};


struct Section {
    std::string                name;
    unsigned long              characteristics;
    std::vector<unsigned char> bytes;
    std::vector<Reloc>         relocs;
    int                        selection;     // IMAGE_COMDAT_SELECT_* of a COMDAT section, 0 otherwise
    int                        associated;    // selection 5 (associative): the 1-based section it goes with
    bool                       isCode;
    unsigned long              align;         // the largest alignment its contents ask for (ALIGN, COMDAT marker)
    Section() : characteristics(0), selection(0), associated(0), isCode(false), align(1) {}
};

struct SymbolEntry {
    std::string    name;
    unsigned long  value;
    int            section;       // 1-based, 0 = undefined
    int            type;          // 0x20 for functions
    int            storageClass;  // 2 external, 3 static
    bool           isSection;     // has one auxiliary record
    int            selection;     // section symbols: the COMDAT selection
    std::string    weakDefault;   // a weak external (";@weak"): the symbol that stands for it when no object defines it
};

// data lines of one section, or of one COMDAT item (a vtable) that becomes a section of its own
struct DataPiece {
    Chunk    chunk;
    Encoder* enc;
    int      kind;       // 2 data, 3 rdata, 4 CRT initialisers, 5 thread-locals (.tls$)
    bool     comdat;
    std::string assoc;   // ";@comdat assoc <symbol>": kept or dropped with the COMDAT data item of that symbol
    DataPiece(int k, bool c) : enc(0), kind(k), comdat(c) { enc = new Encoder(chunk); }
    ~DataPiece() { delete enc; }
};

void put8(std::string& o, int v) { o += static_cast<char>(v & 0xFF); }
void put16(std::string& o, unsigned long v) { put8(o, static_cast<int>(v)); put8(o, static_cast<int>(v >> 8)); }
void put32(std::string& o, unsigned long v) { for (int i = 0; i < 4; ++i) put8(o, static_cast<int>(v >> (8 * i))); }

const unsigned long kCodeFlags  = 0x60500020UL;   // code | execute | read | align 16
const unsigned long kDataFlags  = 0xC0500040UL;   // initialized | read | write | align 16
const unsigned long kRdataFlags = 0x40500040UL;   // initialized | read | align 16
const unsigned long kCrtFlags   = 0x40400040UL;   // initialized | read | align 8
const unsigned long kComdat     = 0x00001000UL;   // IMAGE_SCN_LNK_COMDAT
const int           kSelectAny  = 2;              // IMAGE_COMDAT_SELECT_ANY
const int           kSelectAssociative = 5;       // IMAGE_COMDAT_SELECT_ASSOCIATIVE: kept or dropped with another section
const unsigned long kUnwindFlags = 0x40300040UL;  // .xdata / .pdata: initialized | read | align 4

// bytes of the s-th definition of a chunk: up to the next definition, the start of ALIGN padding or the end
// of the chunk, whichever comes first
unsigned long symbolSize(const Chunk& c, size_t s)
{
    const unsigned long start = c.symbols[s].second;
    unsigned long end = static_cast<unsigned long>(c.bytes.size());
    for (size_t k = 0; k < c.symbols.size(); ++k)
        if (c.symbols[k].second > start && c.symbols[k].second < end) end = c.symbols[k].second;
    for (size_t k = 0; k < c.padStarts.size(); ++k)
        if (c.padStarts[k] >= start && c.padStarts[k] < end) end = c.padStarts[k];
    return end - start;
}

struct SymbolInfoOrder {
    bool operator()(const CoffSymbolInfo& a, const CoffSymbolInfo& b) const
    {
        if (a.section != b.section) return a.section < b.section;
        if (a.offset != b.offset) return a.offset < b.offset;
        return a.name < b.name;
    }
};

// ---- CodeView debug information (.debug$S): line tables and procedure symbols -------------------------------------
//
// link /DEBUG reads them into the PDB, so Visual Studio, WinDbg and dbghelp (SymGetLineFromAddr64) map an address of
// cppparse's code to its source line. The listing carries ";@debug "<file>" <name>" after a PROC and ";@line N"
// before the code of a new line (comments to ml64); the encoder records the code offset of every ;@line.
// The records are built by cppparse/cv: a section's subsections (CvSection), the module's S_OBJNAME and S_COMPILE3
// and a function's S_GPROC32 or S_LPROC32 with its S_END (CvSymbols), its line table (CvLines), the module's file
// table (CvFiles). Here: which sections there are, and their bytes and relocations into the object.
// As cl lays it out: the functions of .text share one .debug$S, which also holds the module's checksum and string
// tables; a COMDAT function gets an associative .debug$S of its own (its symbols and lines) that refers to them.
const unsigned long kDebugFlags = 0x42100040UL;   // .debug$S: initialized | discardable | read | align 1
enum { RelSecRel = 0x000B, RelSection = 0x000A };

// one .debug$S: the bytes of the section 'cv' describes, with the module's file tables when 'files' is given; a
// function's address in them is a SECREL and a SECTION relocation on its symbol
void fillDebugSection(Section& sec, const CvSection& cv, const CvFiles* files)
{
    CvBytes built;
    cv.write(built, files);
    const std::string& bytes = built.bytes();
    const std::vector<CvReloc>& relocs = built.relocs();
    sec.bytes.clear();
    for (size_t i = 0; i < bytes.size(); ++i) sec.bytes.push_back(static_cast<unsigned char>(bytes[i]));
    sec.relocs.clear();
    for (size_t r = 0; r < relocs.size(); ++r) {
        Reloc rl; rl.offset = relocs[r].offset; rl.symbol = relocs[r].symbol; rl.type = RelSection;
        if (relocs[r].kind == CvReloc::SecRel) rl.type = RelSecRel;
        sec.relocs.push_back(rl);
    }
}

} // namespace

// objName: what S_OBJNAME records (the object's file name; empty when no object is written)
// debugInfo: with -g, what the types and variables of the unit are made from (0: the object as it is without -g)
static bool assembleCoffNamed(const std::string& masmText, const std::string& objName, int threads, std::string& objectBytes, CoffResult& result, const CvModule* debugInfo)
{
    std::vector<CodeBlock> blocks;
    std::vector<DataPiece*> pieces;
    DataPiece* plain[6] = { 0, 0, 0, 0, 0, 0 };  // the non-COMDAT piece of each data section kind
    DataPiece* currentPiece = 0;
    std::set<std::string> publics;
    std::vector<std::string> externs;
    std::set<std::string> externSet;
    int section = 0;            // 0 none, 1 text, 2 data, 3 rdata, 4 CRT initialisers, 5 thread-locals
    CodeBlock* current = 0;
    std::map<std::string, std::string> weakDefaults;   // ";@weak <name> <default>": the weak externals and their defaults
    bool comdatNext = false;    // ";@comdat any" was seen: the next PROC or data item is a COMDAT
    unsigned long comdatAlign = 0;   // ";@comdat any align N": the alignment of that item's section
    std::string comdatAssoc;    // ";@comdat assoc <symbol>": the data item goes with that symbol's COMDAT section
    std::string directives;     // ";@drectve <text>": the linker directives of the object (.drectve)
    std::vector<std::string> globalErrors;

    size_t lineStart = 0;
    int lineNo = 0;
    while (lineStart <= masmText.size()) {
        size_t nl = masmText.find('\n', lineStart);
        std::string raw = masmText.substr(lineStart, nl == std::string::npos ? std::string::npos : nl - lineStart);
        lineStart = nl == std::string::npos ? masmText.size() + 1 : nl + 1;
        ++lineNo;
        std::string marker = trimWs(raw);
        if (marker.compare(0, 10, ";@ehrange ") == 0 && current) {   // a try range of the current PROC FRAME (a comment to ml64)
            current->lines.push_back(std::make_pair(lineNo, ".ehrange " + marker.substr(10)));
            continue;
        }
        // cl's exception model (comments to ml64): a funclet's start and its prologue, the FuncInfo's lines
        if (current && (marker.compare(0, 10, ";@funclet ") == 0 || marker.compare(0, 6, ";@ehcl") == 0 || marker.compare(0, 10, ";@ehstate ") == 0 ||
                        marker.compare(0, 8, ";@ehtry ") == 0 || marker.compare(0, 10, ";@ehcatch ") == 0 || marker.compare(0, 7, ";@ehip ") == 0 ||
                        marker.compare(0, 10, ";@pushreg ") == 0 || marker.compare(0, 13, ";@allocstack ") == 0 || marker.compare(0, 13, ";@savexmm128 ") == 0 || marker == ";@endprolog")) {
            current->lines.push_back(std::make_pair(lineNo, "." + marker.substr(2)));
            continue;
        }
        if (marker.compare(0, 7, ";@line ") == 0) {   // the source line of the code that follows (a comment to ml64)
            if (current) current->lines.push_back(std::make_pair(lineNo, ".line " + marker.substr(7)));
            continue;
        }
        if (marker.compare(0, 8, ";@debug ") == 0) {
            // ;@debug "<source file>" <display name>: the function's CodeView symbol and line table (a comment to ml64)
            const size_t q1 = marker.find('"'), q2 = q1 == std::string::npos ? q1 : marker.find('"', q1 + 1);
            if (current && q2 != std::string::npos) { current->sourceFile = marker.substr(q1 + 1, q2 - q1 - 1); current->displayName = trimWs(marker.substr(q2 + 1)); }
            continue;
        }
        // -g (comments to ml64; docs/design-codeview.md 5.4; T<n> is a type's number in the module's CvModule):
        //   ;@proc T<n> <frame> <saved> <flags>     the PROC's type and S_FRAMEPROC's numbers
        //   ;@block <id> <parent>                   a block scope that declares something
        //   ;@local <block> T<n> [rbp-N] <p|l>[r] <name>, ;@gone <block> T<n> <p|l> <name>   a variable, with or without
        //                                           its place in the frame
        //   ;@static <block> T<n> <symbol> <name>   a static local
        //   ;@prologend, ;@epilog, ;@scope <id>     positions in the code: the encoder records their offsets (.cvmark)
        if (current && (marker == ";@prologend" || marker == ";@epilog" || marker.compare(0, 8, ";@scope ") == 0)) {
            current->lines.push_back(std::make_pair(lineNo, ".cvmark " + numText(static_cast<long>(current->positions.size()))));
            current->positions.push_back(marker);
            continue;
        }
        if (marker.compare(0, 7, ";@proc ") == 0) {
            std::istringstream fields(marker.substr(7));
            std::string type;
            fields >> type;
            if (current && type.size() > 1 && type[0] == 'T') {
                current->debug.type = atoi(type.c_str() + 1);
                fields >> current->debug.frameBytes >> current->debug.savedBytes >> current->debug.frameFlags;
            }
            continue;
        }
        if (marker.compare(0, 8, ";@block ") == 0) {
            std::istringstream fields(marker.substr(8));
            int id = 0, parent = 0;
            fields >> id >> parent;
            if (current && id == static_cast<int>(current->debug.blockParents.size()) + 1) current->debug.blockParents.push_back(parent);
            continue;
        }
        if (marker.compare(0, 8, ";@local ") == 0 || marker.compare(0, 7, ";@gone ") == 0) {
            const bool placed = marker[2] == 'l';
            std::istringstream fields(marker.substr(placed ? 8 : 7));
            std::string type, at, kind;
            CvFrameVariable v;
            fields >> v.block >> type;
            if (placed) fields >> at;
            fields >> kind;
            std::getline(fields, v.name);
            v.name = trimWs(v.name);
            v.type = type.size() > 1 && type[0] == 'T' ? atoi(type.c_str() + 1) : 0;
            v.placed = placed;
            v.param = !kind.empty() && kind[0] == 'p';
            v.byAddress = kind.size() > 1 && kind[1] == 'r';
            if (placed && at.size() > 5 && at.compare(0, 4, "[rbp") == 0) v.offset = (at[4] == '-' ? -1 : 1) * atol(at.c_str() + 5);
            if (current && !v.name.empty()) current->debug.variables.push_back(v);
            continue;
        }
        if (marker.compare(0, 9, ";@static ") == 0) {
            std::istringstream fields(marker.substr(9));
            std::string type;
            CvStaticLocal st;
            fields >> st.block >> type >> st.symbol;
            std::getline(fields, st.name);
            st.name = trimWs(st.name);
            st.type = type.size() > 1 && type[0] == 'T' ? atoi(type.c_str() + 1) : 0;
            if (current && st.type != 0 && !st.symbol.empty() && !st.name.empty()) current->debug.statics.push_back(st);
            continue;
        }
        if (marker.compare(0, 10, ";@drectve ") == 0) { directives += trimWs(marker.substr(10)) + " "; continue; }
        if (marker.compare(0, 7, ";@weak ") == 0) {
            // ;@weak <name> <default>: <name> is a weak external of this object. The linker takes <default>, a function
            // defined here, for it unless another object defines <name> (a comment to ml64)
            const std::string rest = trimWs(marker.substr(7));
            const size_t sp = rest.find(' ');
            if (sp != std::string::npos) {
                const std::string name = rest.substr(0, sp);
                weakDefaults[name] = trimWs(rest.substr(sp + 1));
                if (externSet.insert(name).second) externs.push_back(name);
            }
            continue;
        }
        if (marker.compare(0, 8, ";@comdat") == 0) {
            comdatNext = true;
            size_t al = marker.find(" align ");                  // ";@comdat any align 32": the item's section alignment
            comdatAlign = al == std::string::npos ? 0UL : static_cast<unsigned long>(atol(marker.c_str() + al + 7));
            size_t as = marker.find(" assoc ");                  // ";@comdat assoc <symbol>": an associative data item
            comdatAssoc = as == std::string::npos ? std::string() : marker.substr(as + 7, marker.find(' ', as + 7) == std::string::npos ? std::string::npos : marker.find(' ', as + 7) - (as + 7));
            continue;
        }
        size_t sc = raw.find(';');
        if (sc != std::string::npos) raw = raw.substr(0, sc);
        std::string text = trimWs(raw);
        if (text.empty()) continue;
        std::istringstream in(text);
        std::string w0, w1, w2;
        in >> w0 >> w1 >> w2;
        std::string u0 = upperText(w0), u1 = upperText(w1);
        // the prologue directives of a PROC FRAME belong to its code (the other '.' lines are section switches)
        if (current && section == 1 && (u0 == ".PUSHREG" || u0 == ".ALLOCSTACK" || u0 == ".SAVEXMM128" || u0 == ".ENDPROLOG")) {
            current->lines.push_back(std::make_pair(lineNo, text));
            continue;
        }
        if (u0 == ".CODE") { section = 1; currentPiece = 0; continue; }
        if (u0 == ".DATA") { section = 2; currentPiece = 0; continue; }
        if (u0 == ".CONST" || u0 == ".RDATA") { section = 3; currentPiece = 0; continue; }
        if (u1 == "SEGMENT") { section = u0 == "CRT$XCU" ? 4 : TargetTls::isSegmentName(u0) ? 5 : 0; currentPiece = 0; if (section == 0) globalErrors.push_back("line " + numText(lineNo) + ": unknown segment " + w0); continue; }
        if (u1 == "ENDS") { section = 0; currentPiece = 0; continue; }
        if (u0[0] == '.' || u0 == "OPTION" || u0 == "END" || u0 == "INCLUDE" || u0 == "INCLUDELIB") continue;
        if (u0 == "PUBLIC") { publics.insert(w1); if (currentPiece && currentPiece->comdat) currentPiece = 0; continue; }
        if (u0 == "EXTERN" || u0 == "EXTRN") {
            std::string name = w1.substr(0, w1.find(':'));
            if (externSet.insert(name).second) externs.push_back(name);
            continue;
        }
        if (u1 == "PROC") {
            blocks.push_back(CodeBlock());
            blocks.back().name = w0;
            blocks.back().comdat = comdatNext;
            blocks.back().frame = upperText(w2.substr(0, 5)) == "FRAME" && (w2.size() == 5 || w2[5] == ':');
            if (blocks.back().frame && w2.size() > 6) blocks.back().handler = w2.substr(6);
            if (!blocks.back().handler.empty() && externSet.insert(blocks.back().handler).second) externs.push_back(blocks.back().handler);
            comdatNext = false;
            comdatAlign = 0;
            current = &blocks.back();
            continue;
        }
        if (u1 == "ENDP") { current = 0; continue; }
        if (section == 1) {
            if (!current) { globalErrors.push_back("line " + numText(lineNo) + ": code outside PROC: " + text); continue; }
            current->lines.push_back(std::make_pair(lineNo, text));
        } else if (section >= 2 && section <= 5) {
            // a COMDAT data item ends where the next labelled definition begins: a symbol of internal linkage
            // has no PUBLIC line before it, and must not join the COMDAT section the linker may discard
            // (an ALIGN starts the next item too: it pads for the definition after it)
            const bool labelled = u0 != "DB" && u0 != "DW" && u0 != "DD" && u0 != "DQ" && u0 != "REAL4" && u0 != "REAL8" && u0 != "ALIGN";
            // (a label after unlabelled data at the start of a COMDAT item names that item: a vftable's symbol follows
            // its RTTI locator word, as in cl's objects, where the symbol is 8 bytes into its section)
            if (!comdatNext && (labelled || u0 == "ALIGN") && currentPiece && currentPiece->comdat && !currentPiece->chunk.symbols.empty()) currentPiece = 0;
            if (comdatNext) {
                pieces.push_back(new DataPiece(section, true)); currentPiece = pieces.back(); comdatNext = false;
                if (comdatAlign > currentPiece->chunk.maxAlign) currentPiece->chunk.maxAlign = comdatAlign;
                currentPiece->assoc = comdatAssoc;
                comdatAlign = 0; comdatAssoc.clear();
            }
            if (!currentPiece) {
                if (!plain[section]) { pieces.push_back(new DataPiece(section, false)); plain[section] = pieces.back(); }
                currentPiece = plain[section];
            }
            currentPiece->enc->dataLine(text, lineNo);
        } else {
            globalErrors.push_back("line " + numText(lineNo) + ": statement before any section: " + text);
        }
    }

    EncodeJob job;
    job.blocks = &blocks;
    runParallel(job, blocks.size(), threads);

    // sections: .text, .data, .rdata, [.CRT$XCU], then one per COMDAT function and per COMDAT data item
    std::vector<Section> secs(3);
    secs[0].name = ".text";  secs[0].characteristics = kCodeFlags;  secs[0].isCode = true;
    secs[1].name = ".data";  secs[1].characteristics = kDataFlags;
    secs[2].name = ".rdata"; secs[2].characteristics = kRdataFlags;
    if (plain[4]) { Section s; s.name = ".CRT$XCU"; s.characteristics = kCrtFlags; secs.push_back(s); }
    const size_t crtIndex = plain[4] ? 3 : 0;
    // the thread-locals' initial values (cppparse/target TargetTls): only in an object that has some, so that no other
    // object's sections are renumbered
    size_t tlsIndex = 0;
    if (plain[5]) { Section s; s.name = TargetTls::sectionName(); s.characteristics = TargetTls::sectionFlags; tlsIndex = secs.size(); secs.push_back(s); }
    // the unwind info and function table of the functions in .text (a COMDAT function has its own)
    bool plainFrames = false;
    for (size_t i = 0; i < blocks.size(); ++i) if (blocks[i].frame && !blocks[i].comdat) plainFrames = true;
    size_t xdataIndex = 0, pdataIndex = 0;
    if (plainFrames) {
        Section x; x.name = ".xdata"; x.characteristics = kUnwindFlags; xdataIndex = secs.size(); secs.push_back(x);
        Section p; p.name = ".pdata"; p.characteristics = kUnwindFlags; pdataIndex = secs.size(); secs.push_back(p);
    }
    // CodeView: one .debug$S for the functions of .text and the module's file tables (filled once every function is
    // placed); a COMDAT function gets an associative .debug$S of its own in the loop below
    bool anyLines = false;
    for (size_t i = 0; i < blocks.size(); ++i) if (!blocks[i].chunk.lines.empty() && !blocks[i].sourceFile.empty()) anyLines = true;
    size_t debugIndex = 0;
    if (anyLines) { Section d; d.name = ".debug$S"; d.characteristics = kDebugFlags; debugIndex = secs.size(); secs.push_back(d); }
    CvFiles debugFiles;
    CvSection mainDebug;
    if (anyLines) mainDebug.symbols().addModule(objName);
    // -g: the type records, written as the symbols below ask for them, here in this sequential loop only, so that
    // the indices follow the listing's order whatever the number of threads; the variables the object defines
    // (the data lines are encoded as they are read, so their symbols are known before the functions are placed)
    CvModule noModule;
    CvTypes debugTypes(debugInfo ? *debugInfo : noModule);
    std::set<std::string> definedData;
    if (debugInfo)
        for (size_t p = 0; p < pieces.size(); ++p)
            for (size_t k = 0; k < pieces[p]->chunk.symbols.size(); ++k) definedData.insert(pieces[p]->chunk.symbols[k].first);
    std::vector<std::string> debugSymbolRecords;   // the symbol records of every .debug$S, for CvCheck
    const size_t plainSections = secs.size();
    std::vector<SymbolEntry> symbols;
    std::map<std::string, size_t> symbolIndex;   // name -> index in 'symbols'
    // symbols of a COMDAT section follow its section symbol: collected per section, appended at the end
    std::vector<std::vector<SymbolEntry> > comdatSymbols;

    result.errors = globalErrors;
    std::vector<SymbolEntry> plainSymbols;
    unsigned long textBytes = 0;
    for (size_t i = 0; i < blocks.size(); ++i) {
        Chunk& c = blocks[i].chunk;
        result.errors.insert(result.errors.end(), c.errors.begin(), c.errors.end());
        result.instructions += c.instructions;
        textBytes += static_cast<unsigned long>(c.bytes.size());
        Section* target;
        unsigned long base;
        int sectionNumber;
        if (blocks[i].comdat) {
            Section s; s.name = ".text$mn"; s.characteristics = kCodeFlags | kComdat; s.selection = kSelectAny; s.isCode = true;
            secs.push_back(s);
            comdatSymbols.push_back(std::vector<SymbolEntry>());
            target = &secs.back(); base = 0; sectionNumber = static_cast<int>(secs.size());
        } else {
            // every function starts on a 16-byte boundary, the gap filled with int 3, as cl lays out code for
            // speed (/Od, /O2): the section's own 16-byte alignment covers the first one and each COMDAT function
            while (secs[0].bytes.size() % 16) secs[0].bytes.push_back(0xCC);
            target = &secs[0]; base = static_cast<unsigned long>(secs[0].bytes.size()); sectionNumber = 1;
        }
        target->bytes.insert(target->bytes.end(), c.bytes.begin(), c.bytes.end());
        for (size_t r = 0; r < c.relocs.size(); ++r) { Reloc rl = c.relocs[r]; rl.offset += base; target->relocs.push_back(rl); }
        for (size_t s = 0; s < c.symbols.size(); ++s) {
            SymbolEntry f;
            f.name = c.symbols[s].first; f.value = base + c.symbols[s].second; f.section = sectionNumber; f.type = 0x20;
            f.storageClass = publics.count(f.name) ? 2 : 3; f.isSection = false; f.selection = 0;
            if (symbolIndex.count(f.name)) { result.errors.push_back("duplicate symbol " + f.name); continue; }
            symbolIndex[f.name] = 0;   // defined; the index is assigned when the table is laid out
            if (blocks[i].comdat) comdatSymbols.back().push_back(f); else plainSymbols.push_back(f);
            CoffSymbolInfo mi; mi.name = f.name; mi.section = sectionNumber; mi.offset = f.value; mi.size = symbolSize(c, s);
            mi.isFunction = true; mi.external = f.storageClass == 2;
            result.symbolList.push_back(mi);
        }
        if (anyLines && !c.lines.empty() && !blocks[i].sourceFile.empty()) {
            // the function's CodeView symbol and line table: in the associative .debug$S of a COMDAT function, in the
            // module's for one of .text
            const unsigned long codeSize = static_cast<unsigned long>(c.bytes.size());
            const bool internal = !publics.count(blocks[i].name);
            const unsigned long fileEntry = debugFiles.entry(blocks[i].sourceFile);
            const CvLines lines(blocks[i].name, fileEntry, codeSize, c.lines);
            // -g: the procedure's type; a marker that names a type the description does not have is an error of
            // cppparse (CvCheck (a): no T<n> is unresolved)
            CvProcedureMarks& marks = blocks[i].debug;
            unsigned long procType = 0;
            if (debugInfo && marks.type != 0) {
                int bad = marks.type > debugInfo->typeCount() ? marks.type : 0;
                for (size_t v = 0; v < marks.variables.size(); ++v) if (marks.variables[v].type > debugInfo->typeCount()) bad = marks.variables[v].type;
                for (size_t v = 0; v < marks.statics.size(); ++v) if (marks.statics[v].type > debugInfo->typeCount()) bad = marks.statics[v].type;
                if (bad) { result.errors.push_back("CodeView: " + blocks[i].name + " names type T" + numText(bad) + ", which the module's description does not have"); marks.type = 0; }
                else procType = debugTypes.index(marks.type);
            }
            CvSection own;
            CvSymbols& procSymbols = blocks[i].comdat ? own.symbols() : mainDebug.symbols();
            if (debugInfo && marks.type != 0) {
                // -g: the variables and block scopes (CvScopes chooses the form), its static locals each an S_LDATA32 in
                // its block (the object defines the variable: a record names no symbol the object would not have without
                // -g); the frame's places checked against the code the encoder saw (CvCheck (c))
                for (size_t k = 0; k < c.cvMarks.size(); ++k)
                    if (static_cast<size_t>(c.cvMarks[k].second) < blocks[i].positions.size()) marks.positions.push_back(std::make_pair(c.cvMarks[k].first, blocks[i].positions[static_cast<size_t>(c.cvMarks[k].second)]));
                const CvScopes scopes(marks, codeSize, blocks[i].frame && marks.savedBytes != 0);
                procSymbols.beginProcedure(blocks[i].name, blocks[i].displayName, codeSize, internal, procType, scopes.dbgStart(), scopes.dbgEnd(), scopes.procedureFlags());
                scopes.write(procSymbols, debugTypes, blocks[i].name, definedData);
                std::vector<std::pair<unsigned long, unsigned long> > windows;
                windows.push_back(std::make_pair(0UL, c.prologEnd > 0 ? static_cast<unsigned long>(c.prologEnd) : 0UL));
                for (size_t fc = 0; fc < c.funclets.size(); ++fc) windows.push_back(std::make_pair(c.funclets[fc].start, c.funclets[fc].prologEnd > 0 ? static_cast<unsigned long>(c.funclets[fc].prologEnd) : c.funclets[fc].start));
                for (size_t k = 0; k < marks.positions.size(); ++k) {
                    if (marks.positions[k].second != ";@epilog") continue;
                    unsigned long end = codeSize;
                    for (size_t r = 0; r < c.retEnds.size(); ++r) if (c.retEnds[r] > marks.positions[k].first && c.retEnds[r] < end) end = c.retEnds[r];
                    windows.push_back(std::make_pair(marks.positions[k].first, end));
                }
                std::vector<std::string> problems;
                CvCheck::frameRanges(blocks[i].name, scopes.offsetRanges(), windows, problems);
                for (size_t k = 0; k < problems.size() && k < 10; ++k) result.errors.push_back("CodeView: " + problems[k]);
            }
            else procSymbols.beginProcedure(blocks[i].name, blocks[i].displayName, codeSize, internal, procType);
            procSymbols.endProcedure();
            if (blocks[i].comdat) {
                Section d; d.name = ".debug$S"; d.characteristics = kDebugFlags | kComdat; d.selection = kSelectAssociative; d.associated = sectionNumber;
                own.add(lines);
                fillDebugSection(d, own, 0);
                if (debugInfo) debugSymbolRecords.push_back(own.symbols().records().bytes());
                secs.push_back(d);
                comdatSymbols.push_back(std::vector<SymbolEntry>());
            } else mainDebug.add(lines);
        }
        if (!blocks[i].frame || blocks[i].xdata.empty()) continue;
        // UNWIND_INFO ($unwind$f) and RUNTIME_FUNCTION ($pdata$f: begin, end, unwind info as image-relative
        // addresses), named as cl names them; for a COMDAT function in sections that go with its code
        const std::string& fn = blocks[i].name;
        const std::string names[2] = { "$unwind$" + fn, "$pdata$" + fn };
        for (int part = 0; part < 2; ++part) {
            size_t si;
            if (blocks[i].comdat) {
                Section s; s.name = part ? ".pdata" : ".xdata"; s.characteristics = kUnwindFlags | kComdat;
                s.selection = kSelectAssociative; s.associated = sectionNumber;
                si = secs.size(); secs.push_back(s);
                comdatSymbols.push_back(std::vector<SymbolEntry>());
            } else si = part ? pdataIndex : xdataIndex;
            Section& sec = secs[si];
            const unsigned long at = static_cast<unsigned long>(sec.bytes.size());
            if (part == 0) {
                sec.bytes.insert(sec.bytes.end(), blocks[i].xdata.begin(), blocks[i].xdata.end());
                for (size_t r = 0; r < blocks[i].xdataRelocs.size(); ++r) { Reloc rl = blocks[i].xdataRelocs[r]; rl.offset += at; sec.relocs.push_back(rl); }
            }
            else {
                // one entry for the PROC; in cl's exception model one for its own code and one per funclet with a
                // frame, in address order (link refuses a contribution that is not)
                std::vector<EhFrameEntry> entries = blocks[i].entries;
                if (entries.empty()) { EhFrameEntry e; e.begin = 0; e.end = static_cast<unsigned long>(c.bytes.size()); e.unwindAt = 0; entries.push_back(e); }
                for (size_t en = 0; en < entries.size(); ++en) {
                    const unsigned long addend[3] = { entries[en].begin, entries[en].end, entries[en].unwindAt };
                    const std::string relocTo[3] = { fn, fn, names[0] };
                    for (int k = 0; k < 3; ++k) {
                        Reloc rl; rl.offset = static_cast<unsigned long>(sec.bytes.size()); rl.symbol = relocTo[k]; rl.type = RelAddr32Nb;
                        sec.relocs.push_back(rl);
                        for (int b = 0; b < 4; ++b) sec.bytes.push_back(static_cast<unsigned char>((addend[k] >> (8 * b)) & 0xFF));
                    }
                }
            }
            SymbolEntry u;
            u.name = names[part]; u.value = at; u.section = static_cast<int>(si + 1); u.type = 0; u.storageClass = 3; u.isSection = false; u.selection = 0;
            if (symbolIndex.count(u.name)) { result.errors.push_back("duplicate symbol " + u.name); continue; }
            symbolIndex[u.name] = 0;
            if (blocks[i].comdat) comdatSymbols.back().push_back(u); else plainSymbols.push_back(u);
            CoffSymbolInfo mi; mi.name = u.name; mi.section = u.section; mi.offset = at;
            mi.size = static_cast<unsigned long>(sec.bytes.size()) - at; mi.isFunction = false; mi.external = false;
            result.symbolList.push_back(mi);
        }
    }
    result.functions = static_cast<int>(blocks.size());
    std::map<std::string, int> dataSectionOf;   // a data symbol -> its section (for the associative items that follow it)
    for (size_t p = 0; p < pieces.size(); ++p) {
        DataPiece& dp = *pieces[p];
        Chunk& c = dp.chunk;
        result.errors.insert(result.errors.end(), c.errors.begin(), c.errors.end());
        Section* target;
        int sectionNumber;
        if (dp.comdat) {
            // a COMDAT item of .data, .rdata or .CRT$XCU (the entry of a COMDAT variable's dynamic initializer, kept or
            // dropped with the variable's own section, as cl emits it, so that /OPT:REF keeps the one the program uses)
            Section s; s.name = dp.kind == 2 ? ".data" : dp.kind == 4 ? ".CRT$XCU" : ".rdata"; s.characteristics = (dp.kind == 2 ? kDataFlags : dp.kind == 4 ? kCrtFlags : kRdataFlags) | kComdat; s.selection = kSelectAny;
            if (!dp.assoc.empty()) {
                if (dataSectionOf.count(dp.assoc)) { s.selection = kSelectAssociative; s.associated = dataSectionOf[dp.assoc]; }
                else result.errors.push_back("associative COMDAT item: no COMDAT data item named " + dp.assoc + " before it");
            }
            secs.push_back(s);
            comdatSymbols.push_back(std::vector<SymbolEntry>());
            target = &secs.back(); sectionNumber = static_cast<int>(secs.size());
        } else if (dp.kind == 4) { target = &secs[crtIndex]; sectionNumber = static_cast<int>(crtIndex + 1); }
        else if (dp.kind == 5) { target = &secs[tlsIndex]; sectionNumber = static_cast<int>(tlsIndex + 1); }
        else { target = &secs[static_cast<size_t>(dp.kind - 1)]; sectionNumber = dp.kind; }
        unsigned long base = static_cast<unsigned long>(target->bytes.size());
        if (c.maxAlign > target->align) target->align = c.maxAlign;
        target->bytes.insert(target->bytes.end(), c.bytes.begin(), c.bytes.end());
        for (size_t r = 0; r < c.relocs.size(); ++r) { Reloc rl = c.relocs[r]; rl.offset += base; target->relocs.push_back(rl); }
        for (size_t k = 0; k < c.symbols.size(); ++k) {
            SymbolEntry f;
            f.name = c.symbols[k].first; f.value = base + c.symbols[k].second; f.section = sectionNumber; f.type = 0;
            f.storageClass = publics.count(f.name) ? 2 : 3; f.isSection = false; f.selection = 0;
            if (symbolIndex.count(f.name)) { result.errors.push_back("duplicate symbol " + f.name); continue; }
            symbolIndex[f.name] = 0;
            if (dp.comdat) { comdatSymbols.back().push_back(f); dataSectionOf[f.name] = sectionNumber; } else plainSymbols.push_back(f);
            CoffSymbolInfo mi; mi.name = f.name; mi.section = sectionNumber; mi.offset = f.value; mi.size = symbolSize(c, k);
            mi.isFunction = false; mi.external = f.storageClass == 2;
            result.symbolList.push_back(mi);
        }
    }
    for (size_t p = 0; p < pieces.size(); ++p) delete pieces[p];
    pieces.clear();
    // -g (docs/design-codeview.md 5.1, 5.3): the module's variables, constants and type names behind the procedures in
    // the module's .debug$S, a COMDAT variable's record in an associative .debug$S of its own (it goes when the COMDAT
    // goes), the types in .debug$T. A section -g adds comes behind every section the object has without it, .drectve
    // included, and its section symbol behind every symbol: nothing the code's relocations name is renumbered, which
    // is what scripts\cvsame.ps1 checks byte for byte
    std::vector<Section> lateSections;
    std::vector<char> lateHasSymbol;
    if (debugInfo) {
        CvSection lateModule;                    // the module's .debug$S of an object that has none without -g
        if (!anyLines) lateModule.symbols().addModule(objName);
        CvSymbols& moduleSymbols = anyLines ? mainDebug.symbols() : lateModule.symbols();
        const unsigned long before = moduleSymbols.records().size();
        const std::vector<CvVariable>& variables = debugInfo->variables();
        for (size_t v = 0; v < variables.size(); ++v) {
            const CvVariable& var = variables[v];
            // a variable the object defines by its address; one it does not define has no record (a relocation to it
            // would add an undefined external), unless it is a constant: then by its value
            if (!var.symbol.empty() && symbolIndex.count(var.symbol)) {
                const unsigned long type = debugTypes.symbolIndex(var.type);
                std::map<std::string, int>::const_iterator comdat = dataSectionOf.find(var.symbol);
                if (comdat == dataSectionOf.end()) { moduleSymbols.addData(var.symbol, var.name, type, var.internal, var.thread); continue; }
                Section d; d.name = ".debug$S"; d.characteristics = kDebugFlags | kComdat; d.selection = kSelectAssociative; d.associated = comdat->second;
                CvSection own;
                own.symbols().addData(var.symbol, var.name, type, var.internal);
                fillDebugSection(d, own, 0);
                debugSymbolRecords.push_back(own.symbols().records().bytes());
                lateSections.push_back(d); lateHasSymbol.push_back(1);
            }
            else if (var.hasValue) moduleSymbols.addConstant(var.name, debugTypes.symbolIndex(var.type), var.value);
        }
        const std::vector<CvTypedef>& typedefs = debugInfo->typedefs();
        for (size_t t = 0; t < typedefs.size(); ++t) moduleSymbols.addUdt(typedefs[t].name, debugTypes.symbolIndex(typedefs[t].type));
        // every class named so far that the unit has complete gets its definition, and each definition its S_UDT
        debugTypes.finish();
        const std::vector<std::pair<int, unsigned long> >& definitions = debugTypes.definitions();
        for (size_t k = 0; k < definitions.size(); ++k) {
            const CvClass& cls = debugInfo->classAt(definitions[k].first);
            moduleSymbols.addUdt(CvNames::cutName(cls.udtName.empty() ? cls.name : cls.udtName), definitions[k].second);
        }
        debugSymbolRecords.push_back(moduleSymbols.records().bytes());
        if (!anyLines && moduleSymbols.records().size() != before) {
            Section d; d.name = ".debug$S"; d.characteristics = kDebugFlags;
            fillDebugSection(d, lateModule, 0);
            lateSections.push_back(d); lateHasSymbol.push_back(0);
        }
        if (debugTypes.count() != 0) {
            Section t; t.name = ".debug$T"; t.characteristics = kDebugFlags;
            const std::string bytes = debugTypes.section();
            for (size_t k = 0; k < bytes.size(); ++k) t.bytes.push_back(static_cast<unsigned char>(bytes[k]));
            lateSections.push_back(t); lateHasSymbol.push_back(0);
            // the records as they will be written, checked (CvCheck): a violation is an error of cppparse
            std::vector<std::string> problems;
            const unsigned long typeCount = CvCheck::types(bytes, problems);
            for (size_t k = 0; k < debugSymbolRecords.size(); ++k) { CvCheck::symbols(debugSymbolRecords[k], typeCount, problems); CvCheck::procedures(debugSymbolRecords[k], bytes, problems); }
            for (size_t k = 0; k < problems.size() && k < 10; ++k) result.errors.push_back("CodeView: " + problems[k]);
        }
    }
    if (anyLines) fillDebugSection(secs[debugIndex], mainDebug, &debugFiles);
    if (!directives.empty()) {
        // the linker directives (";@drectve"): .drectve, information for link only (LNK_INFO | LNK_REMOVE, align 1),
        // the last section, so that the numbers of the others stay what they were
        Section d;
        d.name = ".drectve";
        d.characteristics = 0x00100A00UL;
        for (size_t i = 0; i < directives.size(); ++i) d.bytes.push_back((unsigned char)directives[i]);
        secs.push_back(d);
        comdatSymbols.push_back(std::vector<SymbolEntry>());
    }
    // what -g adds: behind the last section the object has without it (no entry in comdatSymbols: their section
    // symbols come behind every other symbol, below)
    const size_t lateStart = secs.size();
    secs.insert(secs.end(), lateSections.begin(), lateSections.end());
    // a section whose contents need more than its default alignment (16, or 8 for the initialisers) says so in its
    // IMAGE_SCN_ALIGN_* bits, which the linker honours when it places the section (a 32- or 64-byte aligned variable)
    for (size_t s = 0; s < secs.size(); ++s) {
        const unsigned long bits = (secs[s].characteristics >> 20) & 0xF;
        const unsigned long declared = bits ? 1UL << (bits - 1) : 16UL;
        if (secs[s].align <= declared) continue;
        unsigned long k = 1;
        while ((1UL << (k - 1)) < secs[s].align && k < 14) ++k;
        secs[s].characteristics = (secs[s].characteristics & ~0x00F00000UL) | (k << 20);
    }
    for (size_t s = 0; s < secs.size(); ++s) {
        CoffSectionInfo si; si.number = static_cast<int>(s + 1); si.name = secs[s].name; si.size = static_cast<unsigned long>(secs[s].bytes.size());
        si.isCode = secs[s].isCode; si.comdat = (secs[s].characteristics & kComdat) != 0; si.debug = secs[s].name == ".debug$S" || secs[s].name == ".debug$T";
        result.sectionList.push_back(si);
    }
    std::stable_sort(result.symbolList.begin(), result.symbolList.end(), SymbolInfoOrder());

    // symbol table: the plain section symbols, the plain symbols, each COMDAT section symbol with its symbols, the externals
    for (size_t s = 0; s < plainSections; ++s) {
        SymbolEntry se; se.name = secs[s].name; se.value = 0; se.section = static_cast<int>(s + 1); se.type = 0; se.storageClass = 3; se.isSection = true; se.selection = 0;
        symbols.push_back(se);
    }
    for (size_t i = 0; i < plainSymbols.size(); ++i) { symbolIndex[plainSymbols[i].name] = symbols.size(); symbols.push_back(plainSymbols[i]); }
    for (size_t k = 0; k < comdatSymbols.size(); ++k) {
        size_t s = plainSections + k;
        SymbolEntry se; se.name = secs[s].name; se.value = 0; se.section = static_cast<int>(s + 1); se.type = 0; se.storageClass = 3; se.isSection = true; se.selection = secs[s].selection;
        symbols.push_back(se);
        for (size_t i = 0; i < comdatSymbols[k].size(); ++i) { symbolIndex[comdatSymbols[k][i].name] = symbols.size(); symbols.push_back(comdatSymbols[k][i]); }
    }
    // externals: the declared ones first, then anything referenced but never defined (sorted, for determinism)
    std::set<std::string> undefined;
    for (size_t s = 0; s < secs.size(); ++s)
        for (size_t r = 0; r < secs[s].relocs.size(); ++r) {
            if (symbolIndex.count(secs[s].relocs[r].symbol)) continue;
            // a debug record's address names a symbol the object defines, never an external: debug information must
            // not add a symbol to the table (docs/design-codeview.md 5.1; the relocation rule of CvCheck (b))
            if (secs[s].name == ".debug$S" || secs[s].name == ".debug$T") { result.errors.push_back("CodeView: a debug record's address names " + secs[s].relocs[r].symbol + ", which the object does not define"); continue; }
            if (!externSet.count(secs[s].relocs[r].symbol)) undefined.insert(secs[s].relocs[r].symbol);
        }
    for (size_t e = 0; e < externs.size(); ++e) {
        if (symbolIndex.count(externs[e])) continue;    // declared EXTERN but defined here as well
        SymbolEntry f; f.name = externs[e]; f.value = 0; f.section = 0; f.type = 0; f.storageClass = 2; f.isSection = false; f.selection = 0;
        std::map<std::string, std::string>::const_iterator wk = weakDefaults.find(f.name);
        if (wk != weakDefaults.end()) {
            // IMAGE_SYM_CLASS_WEAK_EXTERNAL: one auxiliary record names the default
            if (!symbolIndex.count(wk->second)) { result.errors.push_back("weak external " + f.name + ": its default " + wk->second + " is not defined in this object"); continue; }
            f.storageClass = 105; f.weakDefault = wk->second;
        }
        symbolIndex[f.name] = symbols.size(); symbols.push_back(f);
    }
    for (std::set<std::string>::const_iterator u = undefined.begin(); u != undefined.end(); ++u) {
        SymbolEntry f; f.name = *u; f.value = 0; f.section = 0; f.type = 0; f.storageClass = 2; f.isSection = false; f.selection = 0;
        symbolIndex[f.name] = symbols.size(); symbols.push_back(f);
    }
    // -g: the section symbols of the associative sections it adds, behind every symbol the object has without it
    for (size_t l = 0; l < lateSections.size(); ++l) {
        if (!lateHasSymbol[l]) continue;
        SymbolEntry se; se.name = secs[lateStart + l].name; se.value = 0; se.section = static_cast<int>(lateStart + l + 1); se.type = 0; se.storageClass = 3; se.isSection = true; se.selection = secs[lateStart + l].selection;
        symbols.push_back(se);
    }
    if (!result.errors.empty()) return false;

    // symbol table indices count auxiliary records
    std::vector<unsigned long> tableIndex(symbols.size());
    unsigned long idx = 0;
    for (size_t i = 0; i < symbols.size(); ++i) { tableIndex[i] = idx; idx += symbols[i].isSection || !symbols[i].weakDefault.empty() ? 2 : 1; }
    unsigned long symbolCount = idx;

    std::string o;
    const size_t n = secs.size();
    unsigned long headerSize = 20 + static_cast<unsigned long>(n) * 40;
    unsigned long offset = headerSize;
    std::vector<unsigned long> rawOffset(n), relocOffset(n), relocCount(n);
    for (size_t s = 0; s < n; ++s) {
        rawOffset[s] = secs[s].bytes.empty() ? 0 : offset;
        offset += static_cast<unsigned long>(secs[s].bytes.size());
        relocCount[s] = static_cast<unsigned long>(secs[s].relocs.size());
        relocOffset[s] = relocCount[s] ? offset : 0;
        offset += (relocCount[s] + (relocCount[s] >= 0xFFFF ? 1 : 0)) * 10;
    }
    unsigned long symbolTableOffset = offset;

    put16(o, 0x8664); put16(o, static_cast<unsigned long>(n)); put32(o, 0); put32(o, symbolTableOffset); put32(o, symbolCount); put16(o, 0); put16(o, 0);
    for (size_t s = 0; s < n; ++s) {
        std::string nm = secs[s].name; nm.resize(8, '\0'); o += nm;
        put32(o, 0); put32(o, 0);                                   // VirtualSize, VirtualAddress
        put32(o, static_cast<unsigned long>(secs[s].bytes.size()));
        put32(o, rawOffset[s]); put32(o, relocOffset[s]); put32(o, 0);
        put16(o, relocCount[s] >= 0xFFFF ? 0xFFFF : relocCount[s]); put16(o, 0);
        put32(o, secs[s].characteristics | (relocCount[s] >= 0xFFFF ? 0x01000000UL : 0));   // LNK_NRELOC_OVFL
    }
    for (size_t s = 0; s < n; ++s) {
        o.append(reinterpret_cast<const char*>(secs[s].bytes.empty() ? 0 : &secs[s].bytes[0]), secs[s].bytes.size());
        if (relocCount[s] >= 0xFFFF) { put32(o, relocCount[s] + 1); put32(o, 0); put16(o, 0); }
        for (size_t r = 0; r < secs[s].relocs.size(); ++r) {
            const Reloc& rl = secs[s].relocs[r];
            put32(o, rl.offset);
            put32(o, tableIndex[symbolIndex[rl.symbol]]);
            put16(o, rl.type);
        }
    }
    std::string strings;
    put32(strings, 0);   // length, patched below
    for (size_t i = 0; i < symbols.size(); ++i) {
        const SymbolEntry& s = symbols[i];
        if (s.name.size() <= 8) { std::string nm = s.name; nm.resize(8, '\0'); o += nm; }
        else { put32(o, 0); put32(o, static_cast<unsigned long>(strings.size())); strings += s.name; strings += '\0'; }
        put32(o, s.value); put16(o, static_cast<unsigned long>(s.section)); put16(o, static_cast<unsigned long>(s.type));
        put8(o, s.storageClass); put8(o, s.isSection || !s.weakDefault.empty() ? 1 : 0);
        if (s.isSection) {
            const Section& sec = secs[static_cast<size_t>(s.section - 1)];
            put32(o, static_cast<unsigned long>(sec.bytes.size()));
            put16(o, sec.relocs.size() >= 0xFFFF ? 0xFFFF : static_cast<unsigned long>(sec.relocs.size()));
            put16(o, 0); put32(o, 0); put16(o, static_cast<unsigned long>(sec.associated)); put8(o, s.selection); put8(o, 0); put8(o, 0); put8(o, 0);
        }
        else if (!s.weakDefault.empty()) {
            // the default's symbol index, then IMAGE_WEAK_EXTERN_SEARCH_ALIAS (3): the default stands for the name unless
            // an object defines it. link.exe then takes the default before it searches the libraries, so the C
            // runtime's function of the same name is not linked (with SEARCH_LIBRARY, 2, as /alternatename gives, it is).
            // SEARCH_NOLIBRARY (1) is no substitute: link.exe honours it only while no other object names the function
            // (it then takes a library's definition, or reports the name unresolved, by the objects' order; measured for
            // row P1, scripts\replace.ps1), and every caller of operator new is such an object
            put32(o, tableIndex[symbolIndex[s.weakDefault]]); put32(o, 3);
            for (int z = 0; z < 10; ++z) put8(o, 0);
        }
    }
    unsigned long sl = static_cast<unsigned long>(strings.size());
    for (int i = 0; i < 4; ++i) strings[i] = static_cast<char>((sl >> (8 * i)) & 0xFF);
    o += strings;

    unsigned long relocations = 0, dataBytes = 0, rdataBytes = 0;
    for (size_t s = 0; s < n; ++s) {
        relocations += static_cast<unsigned long>(secs[s].relocs.size());
        if (secs[s].isCode || secs[s].name == ".debug$S" || secs[s].name == ".debug$T") continue;
        if (secs[s].name == ".data" || secs[s].name == TargetTls::sectionName()) dataBytes += static_cast<unsigned long>(secs[s].bytes.size()); else rdataBytes += static_cast<unsigned long>(secs[s].bytes.size());
    }
    result.relocations = static_cast<int>(relocations);
    result.symbols = static_cast<int>(symbols.size());
    result.textBytes = textBytes;
    result.dataBytes = dataBytes;
    result.rdataBytes = rdataBytes;
    result.fileBytes = static_cast<unsigned long>(o.size());
    objectBytes.swap(o);
    return true;
}

bool assembleCoff(const std::string& masmText, int threads, std::string& objectBytes, CoffResult& result, const CvModule* debugInfo)
{
    return assembleCoffNamed(masmText, std::string(), threads, objectBytes, result, debugInfo);
}

bool writeCoffObject(const std::string& masmText, const std::string& objPath, int threads, CoffResult& result, const CvModule* debugInfo)
{
    std::string bytes;
    // S_OBJNAME holds the object's file name alone: an absolute path would make the object depend on where it is written
    if (!assembleCoffNamed(masmText, objPath.substr(objPath.find_last_of("/\\") + 1), threads, bytes, result, debugInfo)) return false;
    std::ofstream out(objPath.c_str(), std::ios::binary);
    if (!out) { result.errors.push_back("cannot write " + objPath); result.writeFailed = true; return false; }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out.good()) { result.errors.push_back("cannot write " + objPath); result.writeFailed = true; return false; }
    return true;
}

} // namespace cppparse
