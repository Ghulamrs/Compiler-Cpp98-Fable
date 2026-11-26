// CvRegisters.cpp - see CvRegisters.hpp.
#include "CvRegisters.hpp"

#include <cstdlib>

namespace cppparse {

namespace {

// an integer register: its names and CodeView's numbers for its low 8, 16 and 32 bits and for the whole (0: the
// table has none: the parts of rsp and rbp are never a variable's place)
struct IntRegister { const char* n64; const char* n32; const char* n16; const char* n8; int cv8, cv16, cv32, cv64; };

const IntRegister kInt[16] = {
    { "rax", "eax",  "ax",   "al",   1,   9,   17,  328 },
    { "rcx", "ecx",  "cx",   "cl",   2,   10,  18,  330 },
    { "rdx", "edx",  "dx",   "dl",   3,   11,  19,  331 },
    { "rbx", "ebx",  "bx",   "bl",   4,   12,  20,  329 },
    { "rsp", "esp",  "sp",   "spl",  0,   0,   0,   335 },
    { "rbp", "ebp",  "bp",   "bpl",  0,   0,   0,   334 },
    { "rsi", "esi",  "si",   "sil",  324, 15,  23,  332 },
    { "rdi", "edi",  "di",   "dil",  325, 16,  24,  333 },
    { "r8",  "r8d",  "r8w",  "r8b",  344, 352, 360, 336 },
    { "r9",  "r9d",  "r9w",  "r9b",  345, 353, 361, 337 },
    { "r10", "r10d", "r10w", "r10b", 346, 354, 362, 338 },
    { "r11", "r11d", "r11w", "r11b", 347, 355, 363, 339 },
    { "r12", "r12d", "r12w", "r12b", 348, 356, 364, 340 },
    { "r13", "r13d", "r13w", "r13b", 349, 357, 365, 341 },
    { "r14", "r14d", "r14w", "r14b", 350, 358, 366, 342 },
    { "r15", "r15d", "r15w", "r15b", 351, 359, 367, 343 }
};

// xmm0 to xmm15: the low 32 bits (XMMn_0) and the low 64 (XMMnL)
const int kXmmFloat[16]  = { 162, 166, 170, 174, 178, 182, 186, 190, 260, 264, 268, 272, 276, 280, 284, 288 };
const int kXmmDouble[16] = { 194, 195, 196, 197, 198, 199, 200, 201, 292, 293, 294, 295, 296, 297, 298, 299 };

} // namespace

int CvRegisters::family(const std::string& name)
{
    for (int r = 0; r < 16; ++r)
        if (name == kInt[r].n64 || name == kInt[r].n32 || name == kInt[r].n16 || name == kInt[r].n8) return r;
    if (name.size() >= 4 && name.size() <= 5 && name.compare(0, 3, "xmm") == 0) {
        for (size_t i = 3; i < name.size(); ++i) if (name[i] < '0' || name[i] > '9') return -1;
        const int n = std::atoi(name.c_str() + 3);
        if (n >= 0 && n < 16 && !(name.size() == 5 && name[3] == '0')) return 16 + n;
    }
    return -1;
}

int CvRegisters::number(int family, int size)
{
    if (family >= 0 && family < 16) {
        const IntRegister& r = kInt[family];
        return size == 1 ? r.cv8 : size == 2 ? r.cv16 : size == 4 ? r.cv32 : size == 8 ? r.cv64 : 0;
    }
    if (family >= 16 && family < 32) return size == 4 ? kXmmFloat[family - 16] : size == 8 ? kXmmDouble[family - 16] : 0;
    return 0;
}

int CvRegisters::familyOf(int number)
{
    if (number <= 0) return -1;
    for (int r = 0; r < 16; ++r)
        if (number == kInt[r].cv8 || number == kInt[r].cv16 || number == kInt[r].cv32 || number == kInt[r].cv64) return r;
    for (int x = 0; x < 16; ++x)
        if (number == kXmmFloat[x] || number == kXmmDouble[x]) return 16 + x;
    return -1;
}

int CvRegisters::sizeOf(int number)
{
    if (number <= 0) return 0;
    for (int r = 0; r < 16; ++r) {
        if (number == kInt[r].cv8) return 1;
        if (number == kInt[r].cv16) return 2;
        if (number == kInt[r].cv32) return 4;
        if (number == kInt[r].cv64) return 8;
    }
    for (int x = 0; x < 16; ++x) {
        if (number == kXmmFloat[x]) return 4;
        if (number == kXmmDouble[x]) return 8;
    }
    return 0;
}

bool CvRegisters::isVolatile(int family)
{
    if (family >= 16) return family < 16 + 6;                       // xmm0 to xmm5
    return family == 0 || family == 1 || family == 2 || (family >= 8 && family <= 11);   // rax, rcx, rdx, r8 to r11
}

unsigned long CvRegisters::volatileMask()
{
    unsigned long m = 0;
    for (int f = 0; f < Families; ++f) if (isVolatile(f)) m |= bit(f);
    return m;
}

std::string CvRegisters::name(int family)
{
    if (family >= 0 && family < 16) return kInt[family].n64;
    if (family >= 16 && family < 32) {
        const int n = family - 16;
        std::string s = "xmm";
        if (n >= 10) s += '1';
        s += static_cast<char>('0' + n % 10);
        return s;
    }
    return "?";
}

} // namespace cppparse
