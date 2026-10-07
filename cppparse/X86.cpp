// X86.cpp - x86-64 MASM code generation.
#include "X86.hpp"
#include "PathUtil.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>

namespace cppparse {

namespace {

const char* const kIntRegs[] = { "rbx", "rsi", "rdi", "r12", "r13", "r14", "r15" };
const int kIntRegCount = 7;
const int kFloatRegBase = 6;     // xmm6 .. xmm13
const int kFloatRegCount = 8;
const int kTryFrameSize = 272;   // sizeof(jmp_buf) on x64 (256) plus the frame link

// "Derived;Base;GrandBase" for a class (every transitive base), or the scalar type text; the
// runtime matches a catch clause against each entry
void collectBases(const ClassInfo* c, std::string& out, int depth)
{
    if (!c || depth > 16) return;
    for (size_t i = 0; i < c->bases.size(); ++i) {
        const ClassInfo* b = c->bases[i].resolved;
        if (!b) continue;
        out += ";" + b->qualifiedName;
        collectBases(b, out, depth + 1);
    }
}

std::string typeDescriptor(const Type* t)
{
    if (!t) return "unknown";
    if (t->kind == Type::Class && t->cls) { std::string d = t->cls->qualifiedName; collectBases(t->cls, d, 0); return d; }
    return t->str();
}

std::string reg32(const std::string& r)
{
    if (r == "rax") return "eax"; if (r == "rbx") return "ebx"; if (r == "rcx") return "ecx"; if (r == "rdx") return "edx";
    if (r == "rsi") return "esi"; if (r == "rdi") return "edi"; if (r == "r8") return "r8d"; if (r == "r9") return "r9d";
    if (r == "r10") return "r10d"; if (r == "r11") return "r11d";
    return r + "d";   // r12..r15
}

std::string reg16(const std::string& r)
{
    if (r == "rax") return "ax"; if (r == "rbx") return "bx"; if (r == "rcx") return "cx"; if (r == "rdx") return "dx";
    if (r == "rsi") return "si"; if (r == "rdi") return "di";
    return r + "w";   // r8w ... r15w
}

std::string reg8(const std::string& r)
{
    if (r == "rax") return "al"; if (r == "rbx") return "bl"; if (r == "rcx") return "cl"; if (r == "rdx") return "dl";
    if (r == "rsi") return "sil"; if (r == "rdi") return "dil";
    return r + "b";   // r8b ... r15b
}

std::string regSized(const std::string& r, int size)
{
    if (size == 1) return reg8(r);
    if (size == 2) return reg16(r);
    if (size == 4) return reg32(r);
    return r;
}

const char* ptrWord(int size)
{
    if (size == 1) return "byte ptr";
    if (size == 2) return "word ptr";
    if (size == 4) return "dword ptr";
    return "qword ptr";
}

std::string runtimeName(const std::string& sub)
{
    if (sub == "__new" || sub == "__new_array") return "malloc";
    if (sub == "__delete" || sub == "__delete_array") return "free";
    if (sub == "__copy") return "memcpy";
    if (sub == "__throw") return "cppparse_throw";
    if (endsWith(sub, "?")) return lastComponent(sub.substr(0, sub.size() - 1));   // unknown library function: its C name
    if (sub.find('(') == std::string::npos) return sub;                            // extern "C": plain name
    return mangle(sub);
}

std::string mangleGlobal(const std::string& name)
{
    if (startsWith(name, "vtable@")) return "vtable_" + mangle(name.substr(7));
    return "g_" + mangle(name);
}

} // namespace

// ---- per function --------------------------------------------------------------------

class X86Function {
public:
    X86Function(X86Module& m, const IrFunction& f, LowerStats& stats) : m_m(m), m_f(f), m_stats(stats), m_frameSize(0), m_throwSlot(0) {}
    std::string run();

private:
    struct Interval { int temp; int start, end; bool isFloat; int reg; int slot; Interval() : temp(-1), start(1 << 30), end(-1), isFloat(false), reg(-1), slot(0) {} };
    struct ByStart { bool operator()(const Interval* a, const Interval* b) const { return a->start < b->start; } };

    void computeIntervals();
    void allocate();
    void layoutFrame();
    void emitBody();
    void emitInst(const IrInst& in, int blockIndex);
    void emitCall(const IrInst& in);

    bool isFloatType(const Type* t) const { return t && (t->kind == Type::Float || t->kind == Type::Double); }
    bool tempIsFloat(int t) const { return isFloatType(m_f.typeOf(t)); }
    bool opIsFloat(const IrOperand& o) const { return o.kind == IrOperand::FImm || (o.kind == IrOperand::Temp && tempIsFloat(o.id)) || (o.type && isFloatType(o.type) && o.kind != IrOperand::Imm); }
    int  typeSize(const Type* t) const { if (!t) return 8; if (t->kind == Type::Enum || t->kind == Type::Bool) return t->kind == Type::Bool ? 1 : 4; int n = t->builtinSize(); return n > 0 ? n : 8; }
    bool isSingle(const Type* t) const { return t && t->kind == Type::Float; }

    std::string use(const IrOperand& o, const std::string& scratch);     // materialise into a register name
    std::string def(int temp, bool isFloat, const std::string& scratch, int& spill);
    void        finish(int temp, int spill, bool isFloat, const std::string& reg);
    std::string localRef(int local) const { return "[rbp-" + intToString(m_localOffset[static_cast<size_t>(local)]) + "]"; }
    std::string label(int b) const { return m_label + "_B" + intToString(b); }
    void        line(const std::string& s) { m_out << "        " << s << "\n"; ++m_stats.instructions; }
    void        comment(const std::string& s) { m_out << "        ; " << s << "\n"; }
    std::string floatConst(double v);

    X86Module&          m_m;
    const IrFunction&   m_f;
    LowerStats&         m_stats;
    std::ostringstream  m_out;
    std::string         m_label;
    std::vector<std::pair<int, int> > m_linear;
    std::vector<int>    m_blockStart, m_blockEnd;
    std::map<int, Interval> m_intervals;
    std::vector<int>    m_calls;
    std::vector<int>    m_localOffset;
    int                 m_frameSize;
    std::map<long, int> m_tryOffset;      // version 5: try region id -> [rbp-offset] of its setjmp frame
    int                 m_throwSlot;      // [rbp-offset] where a thrown scalar is stored before the call
    std::set<int>       m_usedIntRegs, m_usedFloatRegs;
    std::map<int, int>  m_savedIntOffset, m_savedFloatOffset;
    int                 m_outgoingArgs;
};

void X86Function::computeIntervals()
{
    m_blockStart.assign(m_f.blocks.size(), 0);
    m_blockEnd.assign(m_f.blocks.size(), 0);
    int k = 0;
    for (size_t b = 0; b < m_f.blocks.size(); ++b) {
        m_blockStart[b] = k;
        for (size_t i = 0; i < m_f.blocks[b].insts.size(); ++i) { m_linear.push_back(std::make_pair(static_cast<int>(b), static_cast<int>(i))); ++k; }
        m_blockEnd[b] = k - 1;
    }
    m_outgoingArgs = 0;
    for (size_t p = 0; p < m_linear.size(); ++p) {
        const IrInst& in = m_f.blocks[static_cast<size_t>(m_linear[p].first)].insts[static_cast<size_t>(m_linear[p].second)];
        int pos = static_cast<int>(p);
        if (in.dst >= 0) { Interval& iv = m_intervals[in.dst]; iv.temp = in.dst; iv.isFloat = isFloatType(in.type); if (pos < iv.start) iv.start = pos; if (pos > iv.end) iv.end = pos; }
        for (size_t a = 0; a < in.args.size(); ++a) {
            if (in.args[a].kind != IrOperand::Temp) continue;
            Interval& iv = m_intervals[in.args[a].id];
            iv.temp = in.args[a].id;
            if (iv.start > pos) iv.start = pos;
            if (pos > iv.end) iv.end = pos;
            if (in.args[a].type && isFloatType(in.args[a].type)) iv.isFloat = true;
        }
        if (in.op == IrInst::Call || in.op == IrInst::VCall) {
            m_calls.push_back(pos);
            int n = static_cast<int>(in.args.size());
            if (n - 4 > m_outgoingArgs) m_outgoingArgs = n - 4;
        }
    }
    for (std::map<int, Interval>::iterator it = m_intervals.begin(); it != m_intervals.end(); ++it)
        if (!it->second.isFloat) it->second.isFloat = tempIsFloat(it->first);
    for (size_t b = 0; b < m_f.blocks.size(); ++b)
        for (size_t s = 0; s < m_f.blocks[b].succs.size(); ++s) {
            int target = m_f.blocks[b].succs[s];
            if (target < 0 || static_cast<size_t>(target) >= m_f.blocks.size()) continue;
            if (m_blockStart[static_cast<size_t>(target)] > m_blockStart[b]) continue;
            int loopStart = m_blockStart[static_cast<size_t>(target)], loopEnd = m_blockEnd[b];
            for (std::map<int, Interval>::iterator it = m_intervals.begin(); it != m_intervals.end(); ++it) {
                Interval& iv = it->second;
                if (iv.start < loopStart && iv.end >= loopStart && iv.end < loopEnd) iv.end = loopEnd;
                if (iv.start >= loopStart && iv.start <= loopEnd && iv.end < loopEnd && iv.start > iv.end) iv.end = loopEnd;
            }
        }
}

void X86Function::allocate()
{
    // callee-saved registers survive calls, so intervals need no call-crossing treatment
    std::vector<Interval*> order;
    for (std::map<int, Interval>::iterator it = m_intervals.begin(); it != m_intervals.end(); ++it) order.push_back(&it->second);
    std::sort(order.begin(), order.end(), ByStart());
    std::vector<Interval*> active;
    bool intFree[kIntRegCount], fltFree[kFloatRegCount];
    for (int i = 0; i < kIntRegCount; ++i) intFree[i] = true;
    for (int i = 0; i < kFloatRegCount; ++i) fltFree[i] = true;
    for (size_t k = 0; k < order.size(); ++k) {
        Interval* iv = order[k];
        for (size_t a = 0; a < active.size();) {
            if (active[a]->end < iv->start) {
                if (active[a]->reg >= 0) { if (active[a]->isFloat) fltFree[active[a]->reg] = true; else intFree[active[a]->reg] = true; }
                active.erase(active.begin() + static_cast<long>(a));
            } else ++a;
        }
        bool* freeRegs = iv->isFloat ? fltFree : intFree;
        int n = iv->isFloat ? kFloatRegCount : kIntRegCount;
        int got = -1;
        for (int r = 0; r < n; ++r) if (freeRegs[r]) { got = r; break; }
        if (got >= 0) { iv->reg = got; freeRegs[got] = false; active.push_back(iv); (iv->isFloat ? m_usedFloatRegs : m_usedIntRegs).insert(got); continue; }
        Interval* victim = 0;
        for (size_t a = 0; a < active.size(); ++a)
            if (active[a]->isFloat == iv->isFloat && active[a]->reg >= 0 && (!victim || active[a]->end > victim->end)) victim = active[a];
        if (victim && victim->end > iv->end) { iv->reg = victim->reg; victim->reg = -1; ++m_stats.spills; active.push_back(iv); }
        else { iv->reg = -1; ++m_stats.spills; }
    }
}

void X86Function::layoutFrame()
{
    int off = 0;
    m_localOffset.assign(m_f.locals.size(), 0);
    for (size_t l = 0; l < m_f.locals.size(); ++l) {
        int size = m_f.locals[l].size > 0 ? m_f.locals[l].size : 8;
        int align = m_f.locals[l].align > 0 ? m_f.locals[l].align : 8;
        if (align > 16) align = 16;
        off = (off + size + align - 1) / align * align;
        m_localOffset[l] = off;
    }
    for (std::map<int, Interval>::iterator it = m_intervals.begin(); it != m_intervals.end(); ++it)
        if (it->second.reg < 0) { off = (off + 8 + 7) / 8 * 8; it->second.slot = off; }
    for (std::set<int>::iterator r = m_usedIntRegs.begin(); r != m_usedIntRegs.end(); ++r) { off = (off + 8 + 7) / 8 * 8; m_savedIntOffset[*r] = off; }
    for (std::set<int>::iterator r = m_usedFloatRegs.begin(); r != m_usedFloatRegs.end(); ++r) { off = (off + 16 + 15) / 16 * 16; m_savedFloatOffset[*r] = off; }
    // version 5: one setjmp frame (jmp_buf plus link, 272 bytes, 16-byte aligned) per try region,
    // and a 16-byte slot for thrown scalars
    for (size_t b = 0; b < m_f.blocks.size(); ++b)
        for (size_t i = 0; i < m_f.blocks[b].insts.size(); ++i) {
            const IrInst& in = m_f.blocks[b].insts[i];
            if (in.op == IrInst::TryBegin) { off = (off + kTryFrameSize + 15) / 16 * 16; m_tryOffset[in.extra] = off; }
            else if (in.op == IrInst::Throw && !in.args.empty() && m_throwSlot == 0) { off = (off + 16 + 15) / 16 * 16; m_throwSlot = off; }
        }
    int outgoing = 32 + 8 * m_outgoingArgs;       // shadow space plus stack arguments
    m_frameSize = (off + outgoing + 15) / 16 * 16;
    m_stats.frameSize = m_frameSize;
}

std::string X86Function::floatConst(double v) { return m_m.floatLabel(v); }

namespace {

std::string hex32(unsigned long v)
{
    std::string r;
    for (int i = 7; i >= 0; --i) r += "0123456789abcdef"[(v >> (i * 4)) & 15];
    return r;
}

// label for a double from its IEEE bits (two 32-bit halves: C++98 has no 64-bit integer)
std::string floatName(double v)
{
    unsigned long half[2];
    std::memcpy(half, &v, sizeof half);
    return "fconst_" + hex32(half[1]) + hex32(half[0]);
}

// label for a string literal from two independent 32-bit hashes of its contents
std::string stringName(const std::string& text)
{
    unsigned long fnv = 2166136261UL, djb = 5381UL;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        fnv = (fnv ^ c) * 16777619UL;
        djb = djb * 33UL + c;
    }
    return "str_" + hex32(fnv) + "_" + hex32(djb);
}

} // namespace

std::string X86Module::floatLabel(double v)
{
    std::string name = floatName(v);
    Lock l(m_mutex);
    m_floats[name] = v;
    return name;
}

std::string X86Module::stringLabel(const std::string& text)
{
    std::string name = stringName(text);
    Lock l(m_mutex);
    m_strings[name] = text;
    return name;
}

std::string X86Function::use(const IrOperand& o, const std::string& scratch)
{
    bool fl = opIsFloat(o);
    switch (o.kind) {
    case IrOperand::Temp: {
        std::map<int, Interval>::iterator it = m_intervals.find(o.id);
        if (it == m_intervals.end()) { line("xor " + reg32(scratch) + ", " + reg32(scratch) + "        ; undefined temporary"); return scratch; }
        if (it->second.reg >= 0) return it->second.isFloat ? "xmm" + intToString(kFloatRegBase + it->second.reg) : kIntRegs[it->second.reg];
        if (it->second.isFloat) {
            std::string xr = fl ? scratch : "xmm5";
            line("movsd " + xr + ", real8 ptr [rbp-" + intToString(it->second.slot) + "]");
            return xr;
        }
        line("mov " + scratch + ", qword ptr [rbp-" + intToString(it->second.slot) + "]");
        return scratch;
    }
    case IrOperand::Imm: line("mov " + scratch + ", " + intToString(static_cast<int>(o.ival))); return scratch;
    case IrOperand::FImm: { std::string c = floatConst(o.fval); line("movsd " + scratch + ", real8 ptr [" + c + "]"); return scratch; }
    case IrOperand::Local: line("lea " + scratch + ", " + localRef(o.id)); return scratch;
    case IrOperand::Global: { std::string g = mangleGlobal(o.name); m_m.useGlobal(o.name); line("lea " + scratch + ", " + g); return scratch; }
    case IrOperand::Str: { line("lea " + scratch + ", " + m_m.stringLabel(o.name)); return scratch; }
    default: return scratch;
    }
}

std::string X86Function::def(int temp, bool isFloat, const std::string& scratch, int& spill)
{
    spill = 0;
    std::map<int, Interval>::iterator it = m_intervals.find(temp);
    if (it == m_intervals.end()) return scratch;
    if (it->second.reg >= 0) return it->second.isFloat ? "xmm" + intToString(kFloatRegBase + it->second.reg) : kIntRegs[it->second.reg];
    spill = it->second.slot;
    (void)isFloat;
    return scratch;
}

void X86Function::finish(int temp, int spill, bool isFloat, const std::string& reg)
{
    (void)temp;
    if (spill > 0) line(isFloat ? "movsd real8 ptr [rbp-" + intToString(spill) + "], " + reg : "mov qword ptr [rbp-" + intToString(spill) + "], " + reg);
}

void X86Function::emitCall(const IrInst& in)
{
    // evaluate every argument into its slot: first four into scratch homes, the rest onto the outgoing area
    // (two phases so that materialising one argument cannot clobber another)
    // every argument is staged in its outgoing slot [rsp+8a] (the first four slots are the
    // shadow space), then the first four are loaded into their registers; stack arguments
    // (a >= 4) are already in place
    static const char* const argRegs[] = { "rcx", "rdx", "r8", "r9" };
    std::vector<bool> isFloatArg;
    for (size_t a = 0; a < in.args.size(); ++a) {
        const IrOperand& o = in.args[a];
        bool fl = opIsFloat(o);
        if (fl) { std::string r = use(o, "xmm4"); line("movsd real8 ptr [rsp+" + intToString(8 * static_cast<int>(a)) + "], " + r); }
        else { std::string r = use(o, "rax"); line("mov qword ptr [rsp+" + intToString(8 * static_cast<int>(a)) + "], " + r); }
        isFloatArg.push_back(fl);
    }
    for (size_t a = 0; a < in.args.size() && a < 4; ++a) {
        if (isFloatArg[a]) line("movsd xmm" + intToString(static_cast<int>(a)) + ", real8 ptr [rsp+" + intToString(8 * static_cast<int>(a)) + "]");
        else line("mov " + std::string(argRegs[a]) + ", qword ptr [rsp+" + intToString(8 * static_cast<int>(a)) + "]");
    }
    if (in.op == IrInst::VCall) {
        line("mov rax, qword ptr [rcx]              ; vptr");
        line("call qword ptr [rax+" + intToString(static_cast<int>(in.extra * 8)) + "]        ; slot " + intToString(static_cast<int>(in.extra)) + " " + in.sub);
    } else if (in.sub == "(indirect)") {
        line("call rcx");
    } else {
        std::string callee = runtimeName(in.sub);
        m_m.useExtern(callee);
        line("call " + callee);
    }
    if (in.dst >= 0) {
        bool fl = tempIsFloat(in.dst);
        int spill;
        std::string rd = def(in.dst, fl, fl ? "xmm0" : "rax", spill);
        if (fl) { if (rd != "xmm0") line("movsd " + rd + ", xmm0"); }
        else if (rd != "rax") line("mov " + rd + ", rax");
        finish(in.dst, spill, fl, rd);
    }
}

void X86Function::emitInst(const IrInst& in, int blockIndex)
{
    comment(in.str());
    bool dstFloat = in.dst >= 0 && tempIsFloat(in.dst);
    int spill = 0;
    switch (in.op) {
    case IrInst::Const: { std::string rd = def(in.dst, false, "rax", spill); line("mov " + rd + ", " + intToString(static_cast<int>(in.args[0].ival))); finish(in.dst, spill, false, rd); break; }
    case IrInst::FConst: { std::string rd = def(in.dst, true, "xmm0", spill); line("movsd " + rd + ", real8 ptr [" + floatConst(in.args[0].fval) + "]"); finish(in.dst, spill, true, rd); break; }
    case IrInst::StrConst: { std::string rd = def(in.dst, false, "rax", spill); line("lea " + rd + ", " + m_m.stringLabel(in.args[0].name)); finish(in.dst, spill, false, rd); break; }
    case IrInst::Copy: {
        std::string rs = use(in.args[0], dstFloat ? "xmm1" : "rdx");
        std::string rd = def(in.dst, dstFloat, dstFloat ? "xmm0" : "rax", spill);
        if (rd != rs) line(std::string(dstFloat ? "movsd " : "mov ") + rd + ", " + rs);
        finish(in.dst, spill, dstFloat, rd);
        break;
    }
    case IrInst::LoadLocal: case IrInst::LoadGlobal: case IrInst::LoadMem: {
        int size = typeSize(in.type);
        std::string mem;
        if (in.op == IrInst::LoadLocal) mem = localRef(in.args[0].id);
        else if (in.op == IrInst::LoadGlobal) { mem = "[" + mangleGlobal(in.args[0].name) + "]"; m_m.useGlobal(in.args[0].name); }
        else mem = "[" + use(in.args[0], "rdx") + "]";
        std::string rd = def(in.dst, dstFloat, dstFloat ? "xmm0" : "rax", spill);
        if (dstFloat) line(std::string(isSingle(in.type) ? "movss " : "movsd ") + rd + ", " + (isSingle(in.type) ? "real4 ptr " : "real8 ptr ") + mem);
        else if (size == 8) line("mov " + rd + ", qword ptr " + mem);
        else if (size == 4) line(std::string(in.type && in.type->isUnsigned ? "mov " + reg32(rd) : "movsxd " + rd) + ", dword ptr " + mem);
        else line(std::string(in.type && (in.type->isUnsigned || in.type->kind == Type::Bool) ? "movzx " : "movsx ") + rd + ", " + ptrWord(size) + " " + mem);
        finish(in.dst, spill, dstFloat, rd);
        break;
    }
    case IrInst::StoreLocal: case IrInst::StoreGlobal: case IrInst::StoreMem: {
        int size = typeSize(in.type);
        bool fl = opIsFloat(in.args[1]);
        std::string mem;
        if (in.op == IrInst::StoreLocal) mem = localRef(in.args[0].id);
        else if (in.op == IrInst::StoreGlobal) { mem = "[" + mangleGlobal(in.args[0].name) + "]"; m_m.useGlobal(in.args[0].name); }
        else mem = "[" + use(in.args[0], "rdx") + "]";
        std::string rv = use(in.args[1], fl ? "xmm0" : "rax");
        if (fl) line(std::string(isSingle(in.type) ? "movss " : "movsd ") + (isSingle(in.type) ? "real4 ptr " : "real8 ptr ") + mem + ", " + rv);
        else line("mov " + std::string(ptrWord(size)) + " " + mem + ", " + regSized(rv, size));
        break;
    }
    case IrInst::AddrLocal: { std::string rd = def(in.dst, false, "rax", spill); line("lea " + rd + ", " + localRef(in.args[0].id)); finish(in.dst, spill, false, rd); break; }
    case IrInst::AddrGlobal: { std::string rd = def(in.dst, false, "rax", spill); m_m.useGlobal(in.args[0].name); line("lea " + rd + ", " + mangleGlobal(in.args[0].name)); finish(in.dst, spill, false, rd); break; }
    case IrInst::AddrField: { std::string rb = use(in.args[0], "rdx"); std::string rd = def(in.dst, false, "rax", spill); line("lea " + rd + ", [" + rb + "+" + intToString(static_cast<int>(in.extra)) + "]      ; " + in.sub); finish(in.dst, spill, false, rd); break; }
    case IrInst::AddrIndex: {
        std::string rb = use(in.args[0], "rdx");
        std::string ri = use(in.args[1], "rcx");
        long size = in.extra > 0 ? in.extra : 1;
        std::string rd = def(in.dst, false, "rax", spill);
        if (size == 1 || size == 2 || size == 4 || size == 8) line("lea " + rd + ", [" + rb + "+" + ri + "*" + intToString(static_cast<int>(size)) + "]");
        else { line("imul rax, " + ri + ", " + intToString(static_cast<int>(size))); if (rd != "rax") line("mov " + rd + ", rax"); line("add " + rd + ", " + rb); }
        finish(in.dst, spill, false, rd);
        break;
    }
    case IrInst::Bin: {
        const std::string& s = in.sub;
        if (s[0] == 'f') {
            std::string a = use(in.args[0], "xmm0"), b = use(in.args[1], "xmm1");
            std::string rd = def(in.dst, true, "xmm0", spill);
            bool single = isSingle(in.type);
            std::string mn = s == "fadd" ? "adds" : s == "fsub" ? "subs" : s == "fmul" ? "muls" : "divs";
            mn += single ? "s" : "d";
            if (rd == b && rd != a) { line("movsd xmm2, " + b); b = "xmm2"; }
            if (rd != a) line(std::string(single ? "movss " : "movsd ") + rd + ", " + a);
            line(mn + " " + rd + ", " + b);
            finish(in.dst, spill, true, rd);
            break;
        }
        if (s == "sdiv" || s == "udiv" || s == "srem" || s == "urem") {
            std::string a = use(in.args[0], "rax"), b = use(in.args[1], "rcx");
            if (a != "rax") line("mov rax, " + a);
            if (b == "rax" || b == "rdx") { line("mov rcx, " + b); b = "rcx"; }
            if (s[0] == 's') { line("cqo"); line("idiv " + b); } else { line("xor edx, edx"); line("div " + b); }
            std::string rd = def(in.dst, false, "rax", spill);
            std::string src = (s == "srem" || s == "urem") ? "rdx" : "rax";
            if (rd != src) line("mov " + rd + ", " + src);
            finish(in.dst, spill, false, rd);
            break;
        }
        if (s == "shl" || s == "ashr" || s == "lshr") {
            std::string a = use(in.args[0], "rax");
            std::string rd = def(in.dst, false, "rax", spill);
            if (in.args[1].kind == IrOperand::Imm) {
                if (rd != a) line("mov " + rd + ", " + a);
                line(std::string(s == "shl" ? "shl " : s == "ashr" ? "sar " : "shr ") + rd + ", " + intToString(static_cast<int>(in.args[1].ival)));
            } else {
                std::string b = use(in.args[1], "rcx");
                if (b != "rcx") line("mov rcx, " + b);
                if (rd != a) line("mov " + rd + ", " + a);
                line(std::string(s == "shl" ? "shl " : s == "ashr" ? "sar " : "shr ") + rd + ", cl");
            }
            finish(in.dst, spill, false, rd);
            break;
        }
        std::string a = use(in.args[0], "rax"), b = use(in.args[1], "rdx");
        std::string rd = def(in.dst, false, "rax", spill);
        std::string mn = s == "add" ? "add" : s == "sub" ? "sub" : s == "mul" ? "imul" : s == "and" ? "and" : s == "or" ? "or" : "xor";
        if (rd == b && rd != a) { if (mn == "add" || mn == "imul" || mn == "and" || mn == "or" || mn == "xor") std::swap(a, b); else { line("mov rdx, " + b); b = "rdx"; } }
        if (rd != a) line("mov " + rd + ", " + a);
        line(mn + " " + rd + ", " + b);
        finish(in.dst, spill, false, rd);
        break;
    }
    case IrInst::Un: {
        if (in.sub == "fneg") {
            std::string a = use(in.args[0], "xmm0");
            std::string rd = def(in.dst, true, "xmm0", spill);
            if (rd != a) line("movsd " + rd + ", " + a);
            line("xorpd " + rd + ", xmmword ptr [sign_mask]");
            finish(in.dst, spill, true, rd);
            break;
        }
        std::string a = use(in.args[0], "rax");
        std::string rd = def(in.dst, false, "rax", spill);
        if (rd != a) line("mov " + rd + ", " + a);
        line(std::string(in.sub == "neg" ? "neg " : "not ") + rd);
        finish(in.dst, spill, false, rd);
        break;
    }
    case IrInst::Cmp: {
        bool fl = in.sub[0] == 'f';
        std::string cc;
        std::string s = fl ? in.sub.substr(1) : in.sub;
        if (fl) {
            std::string a = use(in.args[0], "xmm0"), b = use(in.args[1], "xmm1");
            // ucomisd sets CF/ZF like unsigned compares: use above/below conditions
            if (s == "lt" || s == "le") { std::swap(a, b); s = (s == "lt") ? "gt" : "ge"; }
            line("ucomisd " + a + ", " + b);
            cc = s == "eq" ? "e" : s == "ne" ? "ne" : s == "gt" ? "a" : "ae";
        } else {
            std::string a = use(in.args[0], "rax"), b = use(in.args[1], "rdx");
            int size = typeSize(in.args[0].type ? in.args[0].type : m_f.typeOf(in.args[0].kind == IrOperand::Temp ? in.args[0].id : -1));
            line("cmp " + regSized(a, size) + ", " + regSized(b, size));
            cc = s == "eq" ? "e" : s == "ne" ? "ne" : s == "slt" ? "l" : s == "sle" ? "le" : s == "sgt" ? "g" : s == "sge" ? "ge" : s == "ult" ? "b" : s == "ule" ? "be" : s == "ugt" ? "a" : "ae";
        }
        std::string rd = def(in.dst, false, "rax", spill);
        line("set" + cc + " " + reg8(rd));
        line("movzx " + reg32(rd) + ", " + reg8(rd));
        finish(in.dst, spill, false, rd);
        break;
    }
    case IrInst::Conv: {
        const std::string& s = in.sub;
        if (s == "sitofp" || s == "uitofp") {
            std::string a = use(in.args[0], "rax");
            std::string rd = def(in.dst, true, "xmm0", spill);
            line(std::string(isSingle(in.type) ? "cvtsi2ss " : "cvtsi2sd ") + rd + ", " + a);
            finish(in.dst, spill, true, rd);
            break;
        }
        if (s == "fptosi") {
            std::string a = use(in.args[0], "xmm0");
            std::string rd = def(in.dst, false, "rax", spill);
            line(std::string(isSingle(in.args[0].type) ? "cvttss2si " : "cvttsd2si ") + rd + ", " + a);
            finish(in.dst, spill, false, rd);
            break;
        }
        if (s == "fpext" || s == "fptrunc") {
            std::string a = use(in.args[0], "xmm0");
            std::string rd = def(in.dst, true, "xmm0", spill);
            line(std::string(s == "fpext" ? "cvtss2sd " : "cvtsd2ss ") + rd + ", " + a);
            finish(in.dst, spill, true, rd);
            break;
        }
        std::string a = use(in.args[0], "rax");
        std::string rd = def(in.dst, false, "rax", spill);
        int from = typeSize(in.args[0].type), to = typeSize(in.type);
        if (s == "sext" && from < 8) line(std::string(from == 4 ? "movsxd " : "movsx ") + rd + ", " + regSized(a, from));
        else if (s == "zext" && from < 8) { if (from == 4) line("mov " + reg32(rd) + ", " + reg32(a)); else line("movzx " + rd + ", " + regSized(a, from)); }
        else if (s == "trunc") { if (rd != a) line("mov " + rd + ", " + a); if (to == 4) line("mov " + reg32(rd) + ", " + reg32(rd)); else if (to == 2) line("movzx " + reg32(rd) + ", " + reg16(rd)); else if (to == 1) line("movzx " + reg32(rd) + ", " + reg8(rd)); }
        else if (rd != a) line("mov " + rd + ", " + a);
        finish(in.dst, spill, false, rd);
        break;
    }
    case IrInst::Call: case IrInst::VCall: emitCall(in); break;
    case IrInst::Br: if (in.args[0].id != blockIndex + 1) line("jmp " + label(in.args[0].id)); break;
    case IrInst::CondBr: {
        std::string rc = use(in.args[0], "rax");
        line("test " + reg32(rc) + ", " + reg32(rc));
        line("jne " + label(in.args[1].id));
        if (in.args[2].id != blockIndex + 1) line("jmp " + label(in.args[2].id));
        break;
    }
    case IrInst::Ret: {
        if (!in.args.empty()) {
            bool fl = opIsFloat(in.args[0]);
            std::string r = use(in.args[0], fl ? "xmm0" : "rax");
            if (fl && r != "xmm0") line("movsd xmm0, " + r);
            else if (!fl && r != "rax") line("mov rax, " + r);
        }
        line("jmp " + m_label + "_exit");
        break;
    }
    case IrInst::Throw: {
        // cppparse_throw(object address, type descriptor, size): copies the object and longjmps to the innermost frame
        if (in.args.empty()) { m_m.useExtern("cppparse_rethrow"); line("call cppparse_rethrow            ; throw;"); break; }
        const Type* t = in.args[0].type;
        bool cls = t && t->kind == Type::Class;
        std::string slot = "[rbp-" + intToString(m_throwSlot) + "]";
        if (cls) { std::string r = use(in.args[0], "rcx"); if (r != "rcx") line("mov rcx, " + r); }
        else if (opIsFloat(in.args[0])) { std::string r = use(in.args[0], "xmm0"); line("movsd real8 ptr " + slot + ", " + r); line("lea rcx, " + slot); }
        else { std::string r = use(in.args[0], "rax"); line("mov qword ptr " + slot + ", " + r); line("lea rcx, " + slot); }
        line("lea rdx, " + m_m.stringLabel("\"" + typeDescriptor(t) + "\""));
        line("mov r8, " + intToString(cls ? m_m.m_layouts.sizeOf(t) : 8));
        m_m.useExtern("cppparse_throw");
        line("call cppparse_throw");
        break;
    }
    case IrInst::TryBegin: {
        // register the frame, then _setjmp(frame, NULL): 0 now, 1 when a throw lands here
        std::string fr = "[rbp-" + intToString(m_tryOffset[in.extra]) + "]";
        m_m.useExtern("cppparse_push_frame");
        m_m.useExtern("_setjmp");
        line("lea rcx, " + fr);
        line("call cppparse_push_frame");
        line("lea rcx, " + fr);
        line("xor edx, edx");
        line("call _setjmp");
        line("test eax, eax");
        line("jne " + m_label + "_T" + intToString(static_cast<int>(in.extra)) + "        ; exception: dispatch to the handlers");
        break;
    }
    case IrInst::TryEnd: {
        m_m.useExtern("cppparse_pop_frame");
        line("lea rcx, [rbp-" + intToString(m_tryOffset[in.extra]) + "]");
        line("call cppparse_pop_frame");
        break;
    }
    case IrInst::CatchParam: {
        std::string rd = def(in.dst, false, "rax", spill);
        m_m.useExtern("cppparse_catch_begin");
        line("call cppparse_catch_begin       ; catch (" + in.sub + "): rax = object");
        if (rd != "rax") line("mov " + rd + ", rax");
        finish(in.dst, spill, false, rd);
        break;
    }
    case IrInst::CatchEnd: { m_m.useExtern("cppparse_catch_end"); line("call cppparse_catch_end"); break; }
    case IrInst::Unreachable: line("ud2                 ; " + in.sub); break;
    default: break;
    }
}

void X86Function::emitBody()
{
    m_out << m_label << " PROC\n";
    m_out << "        ; " << m_f.name << "\n";
    m_out << "        ; frame " << m_frameSize << " bytes";
    for (size_t l = 0; l < m_f.locals.size(); ++l) {
        if (l && l % 6 == 0) m_out << "\n        ;   ";
        m_out << (l ? ", " : ": ") << m_f.locals[l].name << "@[rbp-" << m_localOffset[l] << "]";
    }
    m_out << "\n";
    line("push rbp");
    line("mov rbp, rsp");
    line("sub rsp, " + intToString(m_frameSize));
    for (std::map<int, int>::iterator r = m_savedIntOffset.begin(); r != m_savedIntOffset.end(); ++r) line("mov qword ptr [rbp-" + intToString(r->second) + "], " + std::string(kIntRegs[r->first]));
    for (std::map<int, int>::iterator r = m_savedFloatOffset.begin(); r != m_savedFloatOffset.end(); ++r) line("movdqu xmmword ptr [rbp-" + intToString(r->second) + "], xmm" + intToString(kFloatRegBase + r->first));
    // incoming parameters by position
    static const char* const argRegs[] = { "rcx", "rdx", "r8", "r9" };
    for (int p = 0; p < m_f.paramCount; ++p) {
        const IrLocal& l = m_f.locals[static_cast<size_t>(p)];
        bool fl = isFloatType(l.type);
        int size = l.type && l.type->kind != Type::Reference && l.type->kind != Type::Class ? typeSize(l.type) : 8;
        if (p < 4) {
            if (fl) line(std::string(isSingle(l.type) ? "movss " : "movsd ") + (isSingle(l.type) ? "real4 ptr " : "real8 ptr ") + localRef(p) + ", xmm" + intToString(p) + "      ; param " + l.name);
            else line("mov " + std::string(ptrWord(size)) + " " + localRef(p) + ", " + regSized(argRegs[p], size) + "      ; param " + l.name);
        } else {
            std::string home = "[rbp+" + intToString(16 + 8 * p) + "]";
            if (fl) { line("movsd xmm0, real8 ptr " + home); line("movsd real8 ptr " + localRef(p) + ", xmm0      ; param " + l.name); }
            else { line("mov rax, qword ptr " + home); line("mov " + std::string(ptrWord(size)) + " " + localRef(p) + ", " + regSized("rax", size) + "      ; param " + l.name); }
        }
    }
    for (size_t b = 0; b < m_f.blocks.size(); ++b) {
        m_out << label(static_cast<int>(b)) << ":" << (m_f.blocks[b].label.empty() ? "" : "        ; " + m_f.blocks[b].label) << "\n";
        for (size_t i = 0; i < m_f.blocks[b].insts.size(); ++i) emitInst(m_f.blocks[b].insts[i], static_cast<int>(b));
    }
    m_out << m_label << "_exit:\n";
    for (std::map<int, int>::iterator r = m_savedIntOffset.begin(); r != m_savedIntOffset.end(); ++r) line("mov " + std::string(kIntRegs[r->first]) + ", qword ptr [rbp-" + intToString(r->second) + "]");
    for (std::map<int, int>::iterator r = m_savedFloatOffset.begin(); r != m_savedFloatOffset.end(); ++r) line("movdqu xmm" + intToString(kFloatRegBase + r->first) + ", xmmword ptr [rbp-" + intToString(r->second) + "]");
    line("leave");
    line("ret");
    // version 5: one dispatch stub per try region: pop the frame, try the catch clauses in order, else rethrow
    for (size_t b = 0; b < m_f.blocks.size(); ++b)
        for (size_t i = 0; i < m_f.blocks[b].insts.size(); ++i) {
            const IrInst& in = m_f.blocks[b].insts[i];
            if (in.op != IrInst::TryBegin) continue;
            m_out << m_label << "_T" << in.extra << ":        ; handlers of try region " << in.extra << "\n";
            m_m.useExtern("cppparse_pop_frame");
            m_m.useExtern("cppparse_match");
            m_m.useExtern("cppparse_rethrow");
            line("lea rcx, [rbp-" + intToString(m_tryOffset[in.extra]) + "]");
            line("call cppparse_pop_frame");
            bool catchAll = false;
            for (size_t h = 0; h < in.args.size() && !catchAll; ++h) {
                int hb = in.args[h].id;
                std::string ctype = "...";
                const IrBlock& blk = m_f.blocks[static_cast<size_t>(hb)];
                for (size_t k = 0; k < blk.insts.size(); ++k) if (blk.insts[k].op == IrInst::CatchParam) { ctype = blk.insts[k].sub; break; }
                if (ctype == "...") { line("jmp " + label(hb) + "        ; catch (...)"); catchAll = true; break; }
                line("lea rcx, " + m_m.stringLabel("\"" + ctype + "\""));
                line("call cppparse_match");
                line("test eax, eax");
                line("jne " + label(hb) + "        ; catch (" + ctype + ")");
            }
            if (!catchAll) line("call cppparse_rethrow            ; no clause matched: outer frame");
        }
    m_out << m_label << " ENDP\n\n";
}

std::string X86Function::run()
{
    m_label = mangle(m_f.name);
    computeIntervals();
    allocate();
    layoutFrame();
    emitBody();
    return m_out.str();
}

// ---- module ---------------------------------------------------------------------------------

X86Module::X86Module(const CodeBase& cb, TypeTable& types, Layouts& layouts) : m_cb(cb), m_types(types), m_layouts(layouts) {}

void X86Module::addFunction(const IrFunction& f, LowerStats& stats) { appendCode(codeFor(f, stats)); }

std::string X86Module::codeFor(const IrFunction& f, LowerStats& stats)
{
    { Lock l(m_mutex); m_defined.insert(mangle(f.name)); }
    X86Function fn(*this, f, stats);
    return fn.run();
}

void X86Module::appendCode(const std::string& code) { Lock l(m_mutex); m_code += code; }

void X86Module::emitVtables()
{
    const std::vector<ClassInfo*>& classes = m_cb.classes();
    for (size_t i = 0; i < classes.size(); ++i) {
        const ClassInfo* c = classes[i];
        if (!c->isDefinition || c->tmpl.isTemplate || !m_layouts.hasVtable(c)) continue;
        std::string desc = m_layouts.describe(c);
        size_t vt = desc.find("vtable:");
        if (vt == std::string::npos) continue;
        // slot order from the layout description: "[n] key"
        std::vector<std::string> keys;
        std::string rest = desc.substr(vt + 7);
        size_t pos = 0;
        while ((pos = rest.find('[', pos)) != std::string::npos) {
            size_t close = rest.find(']', pos);
            size_t next = rest.find('[', close);
            std::string key = trim(rest.substr(close + 1, next == std::string::npos ? std::string::npos : next - close - 1));
            keys.push_back(key);
            pos = close + 1;
        }
        std::string table = "vtable_" + mangle(c->qualifiedName) + " LABEL QWORD\n";
        for (size_t k = 0; k < keys.size(); ++k) {
            // find the most derived implementation among c and its bases
            const FunctionInfo* impl = 0;
            std::string key = keys[k];
            if (key.find("::") != std::string::npos) key = key.substr(key.find("::") + 2);
            for (const ClassInfo* walk = c; walk && !impl; ) {
                for (size_t m = 0; m < walk->methods.size(); ++m) {
                    const FunctionInfo& fn = walk->methods[m];
                    std::string mk = fn.isDestructor ? "~dtor" : fn.name + "(";
                    if (!fn.isDestructor) { for (size_t p = 0; p < fn.params.size(); ++p) { std::string t; for (size_t ch = 0; ch < fn.params[p].type.size(); ++ch) if (fn.params[p].type[ch] != ' ') t += fn.params[p].type[ch]; mk += t + ";"; } mk += ")"; if (fn.isConst) mk += "c"; }
                    if (mk == key && (fn.definition || fn.hasBody)) { impl = fn.definition ? fn.definition : &fn; break; }
                }
                walk = walk->bases.empty() ? 0 : walk->bases[0].resolved;
            }
            std::string sym = impl ? mangle(impl->displayName() + "(" + impl->paramText + ")" + (impl->isConst ? " const" : "")) : "";
            if (!sym.empty() && !m_defined.count(sym)) m_externs.insert(sym);
            table += "        DQ " + (sym.empty() ? std::string("0") : "OFFSET " + sym) + "        ; [" + intToString(static_cast<int>(k)) + "] " + keys[k] + "\n";
        }
        m_data += table;
    }
}

void X86Module::emitGlobals()
{
    const std::vector<SourceFile*>& files = m_cb.files();
    std::set<std::string> done;
    for (size_t f = 0; f < files.size(); ++f)
        for (size_t v = 0; v < files[f]->variables.size(); ++v) {
            const VariableInfo& g = files[f]->variables[v];
            std::string name = g.qualifier.empty() ? g.fullName() : g.fullName();
            if (g.isExtern || !done.insert(name).second) continue;
            const Type* t = m_types.parse(g.type, g.scope, 0);
            int size = m_layouts.sizeOf(t);
            std::string sym = mangleGlobal(name);
            std::string init = trim(g.initializer);
            bool numeric = !init.empty() && (isdigit(static_cast<unsigned char>(init[0])) || init[0] == '-' || init[0] == '+') && init.find_first_of("(,") == std::string::npos;
            std::string def;
            if (t && t->isFloating()) def = sym + " REAL8 " + (numeric ? init : std::string("0.0"));
            else if (size == 1) def = sym + " DB " + (numeric ? init : std::string("0"));
            else if (size == 2) def = sym + " DW " + (numeric ? init : std::string("0"));
            else if (size == 4) def = sym + " DD " + (numeric ? init : std::string("0"));
            else if (size == 8) def = sym + " DQ " + (numeric ? init : std::string("0"));
            else def = sym + " DB " + intToString(size) + " DUP (0)";
            m_data += "        PUBLIC " + sym + "\n" + def + "\n";
            m_defined.insert(sym);
        }
}

std::string X86Module::finish()
{
    emitGlobals();
    emitVtables();
    std::ostringstream s;
    s << "; cppparse version 4 - x86-64 code for the Windows x64 ABI (MASM syntax, ml64)\n";
    s << "; assemble: ml64 /c /Fo module.obj module.asm\n\n";
    s << "OPTION CASEMAP:NONE\n\n";
    for (std::set<std::string>::iterator e = m_externs.begin(); e != m_externs.end(); ++e)
        if (!m_defined.count(*e)) s << "EXTERN " << *e << ":PROC\n";
    for (std::set<std::string>::iterator g = m_globalsUsed.begin(); g != m_globalsUsed.end(); ++g) {
        std::string sym = mangleGlobal(*g);
        if (!m_defined.count(sym) && !startsWith(*g, "vtable@")) { s << "EXTERN " << sym << ":QWORD\n"; m_defined.insert(sym); }
    }
    s << "\n.data\n" << m_data;
    s << "\n.const\n";
    s << "sign_mask DQ 8000000000000000h, 8000000000000000h\n";
    for (std::map<std::string, double>::const_iterator fi = m_floats.begin(); fi != m_floats.end(); ++fi) {
        std::ostringstream v; v.setf(std::ios::fixed); v.precision(17); v << fi->second;
        s << fi->first << " REAL8 " << v.str() << "\n";
    }
    for (std::map<std::string, std::string>::const_iterator si = m_strings.begin(); si != m_strings.end(); ++si) {
        // the literal is stored with quotes and escapes: emit raw bytes
        std::string lit = si->second;
        if (lit.size() >= 2 && lit[0] == '"') lit = lit.substr(1, lit.size() - 2);
        // MASM limits a line to 512 characters: emit at most 24 bytes per DB line
        std::vector<int> bytes;
        for (size_t c = 0; c < lit.size(); ++c) {
            unsigned char ch = static_cast<unsigned char>(lit[c]);
            if (ch == '\\' && c + 1 < lit.size()) {
                char x = lit[++c];
                ch = x == 'n' ? 10 : x == 't' ? 9 : x == 'r' ? 13 : x == '0' ? 0 : x == '\\' ? '\\' : x == '"' ? '"' : static_cast<unsigned char>(x);
            }
            bytes.push_back(static_cast<int>(ch));
        }
        bytes.push_back(0);
        s << si->first << " LABEL BYTE\n";
        for (size_t c = 0; c < bytes.size(); c += 24) {
            s << "        DB ";
            for (size_t k = c; k < bytes.size() && k < c + 24; ++k) s << (k == c ? "" : ", ") << bytes[k];
            s << "\n";
        }
    }
    s << "\n.code\n";
    for (std::set<std::string>::iterator d = m_defined.begin(); d != m_defined.end(); ++d)
        if (!startsWith(*d, "g_") && !startsWith(*d, "vtable_")) s << "PUBLIC " << *d << "\n";
    s << "\n" << m_code << "END\n";
    return s.str();
}

} // namespace cppparse
