// CvSymbols.cpp - see CvSymbols.hpp.
#include "CvSymbols.hpp"

namespace cppparse {

void CvSymbols::addModule(const std::string& objName)
{
    CvBytes b;
    b.put32(0); b.name(objName);
    m_records.symbolRecord(0x1101, b);
    CvBytes c;
    c.put32(1);                                      // flags: the language, CV_CFL_CXX
    c.put16(0xD0);                                   // machine: CV_CFL_X64
    for (int k = 0; k < 8; ++k) c.put16(k == 0 || k == 4 ? 6 : 0);   // front end 6.0.0.0, back end 6.0.0.0
    c.name("cppparse 6");
    m_records.symbolRecord(0x113C, c);
}

void CvSymbols::beginProcedure(const std::string& fn, const std::string& display, unsigned long codeSize, bool internal, unsigned long typeIndex,
                               unsigned long dbgStart, unsigned long dbgEnd, int flags)
{
    CvBytes b;
    b.put32(0); b.put32(0); b.put32(0);              // pParent, pEnd, pNext: the linker fills them in
    b.put32(codeSize); b.put32(dbgStart); b.put32(dbgEnd == ~0UL ? codeSize : dbgEnd);   // len, DbgStart, DbgEnd
    b.put32(typeIndex);                              // typind: T_NOTYPE (0) without type information
    b.secRel(fn);                                    // off
    b.section(fn);                                   // seg
    b.put8(flags);                                   // flags (80h: optimised debug information, the ranged form)
    b.name(display);
    m_records.symbolRecord(internal ? 0x110F : 0x1110, b);
}

void CvSymbols::addFrameProc(unsigned long frameBytes, unsigned long savedBytes, unsigned long flags)
{
    CvBytes b;
    b.put32(frameBytes); b.put32(0); b.put32(0);     // cbFrame, cbPad, offPad
    b.put32(savedBytes);                             // cbSaveRegs
    b.put32(0); b.put16(0);                          // offExHdlr, sectExHdlr
    b.put32(flags);
    m_records.symbolRecord(0x1012, b);
}

void CvSymbols::addRegRel(long offset, unsigned long typeIndex, int reg, const std::string& name)
{
    CvBytes b;
    b.put32(static_cast<unsigned long>(offset)); b.put32(typeIndex); b.put16(static_cast<unsigned long>(reg));
    b.name(name);
    m_records.symbolRecord(0x1111, b);
}

void CvSymbols::beginBlock(const std::string& fn, unsigned long start, unsigned long length)
{
    CvBytes b;
    b.put32(0); b.put32(0);                          // pParent, pEnd: the linker fills them in
    b.put32(length);
    b.secRel(fn, start); b.section(fn);
    b.name("");
    m_records.symbolRecord(0x1103, b);
}

void CvSymbols::endBlock()
{
    m_records.symbolRecord(0x0006, CvBytes());       // S_END
}

void CvSymbols::addLocal(unsigned long typeIndex, int flags, const std::string& name)
{
    CvBytes b;
    b.put32(typeIndex); b.put16(static_cast<unsigned long>(flags));
    b.name(name);
    m_records.symbolRecord(0x113E, b);
}

void CvSymbols::addFramePointerFullScope(long offset)
{
    CvBytes b;
    b.put32(static_cast<unsigned long>(offset));
    m_records.symbolRecord(0x1144, b);
}

void CvSymbols::addRegisterRel(int reg, long offset, const std::string& fn, unsigned long start, unsigned long length,
                               const std::vector<std::pair<unsigned long, unsigned long> >& gaps)
{
    CvBytes b;
    b.put16(static_cast<unsigned long>(reg)); b.put16(0);   // baseReg; spilledUdtMember 0, offsetParent 0
    b.put32(static_cast<unsigned long>(offset));
    b.secRel(fn, start); b.section(fn); b.put16(length);
    for (size_t g = 0; g < gaps.size(); ++g) { b.put16(gaps[g].first); b.put16(gaps[g].second); }
    m_records.symbolRecord(0x1145, b);
}

void CvSymbols::addRegister(int reg, const std::string& fn, unsigned long start, unsigned long length,
                            const std::vector<std::pair<unsigned long, unsigned long> >& gaps)
{
    CvBytes b;
    b.put16(static_cast<unsigned long>(reg)); b.put16(0);   // reg; attr: not "maybe"
    b.secRel(fn, start); b.section(fn); b.put16(length);
    for (size_t g = 0; g < gaps.size(); ++g) { b.put16(gaps[g].first); b.put16(gaps[g].second); }
    m_records.symbolRecord(0x1141, b);
}

void CvSymbols::endProcedure()
{
    m_records.symbolRecord(0x0006, CvBytes());       // S_END
}

void CvSymbols::addData(const std::string& symbol, const std::string& display, unsigned long typeIndex, bool internal, bool thread)
{
    CvBytes b;
    b.put32(typeIndex);
    b.secRel(symbol);
    b.section(symbol);
    b.name(display);
    m_records.symbolRecord(thread ? (internal ? 0x1112 : 0x1113) : (internal ? 0x110C : 0x110D), b);   // S_LTHREAD32, S_GTHREAD32: the same fields
}

void CvSymbols::addConstant(const std::string& display, unsigned long typeIndex, int64 value)
{
    CvBytes b;
    b.put32(typeIndex);
    b.numeric(value, false);
    b.name(display);
    m_records.symbolRecord(0x1107, b);
}

void CvSymbols::addUdt(const std::string& display, unsigned long typeIndex)
{
    CvBytes b;
    b.put32(typeIndex);
    b.name(display);
    m_records.symbolRecord(0x1108, b);
}

} // namespace cppparse
