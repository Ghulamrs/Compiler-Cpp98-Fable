// EhModel.hpp - the exception handling models of the code generator (--eh=).
//
// The folder cppparse/eh holds what is specific to exception handling, as classes the code generator (X86.cpp), the
// object writer (Coff.cpp) and the IR generator (IrGen.cpp) call:
//   EhRegions      the try and cleanup regions of a function: the stack of regions at each block, the exception edges
//   EhEmitter      the interface the models write assembly through (X86.cpp's function emitter implements it)
//   EhSjlj         the setjmp model's code (version 5): a frame per region, pushed and popped on the normal path
//   EhRanges       the table model's code ranges and their ";@ehrange" lines
//   EhPads         the landing pads of both models: the dispatch stub of a try region, a handler's begin and end
//   EhThrow        a throw's call of the runtime
//   EhUnwindInfo   the bytes of a Windows x64 UNWIND_INFO from a prologue's steps
//   EhRangeTable   the table model's range table ("CPPX") behind the unwind info
//   EhClThrowInfo  cl's model: the ThrowInfo, CatchableTypeArray and CatchableTypes of a thrown type
// The regions themselves, their landing pads' contents and the order of destruction are the language's semantics and
// stay in IrGen.cpp; the register allocator and the frame layout stay in X86.cpp and ask these classes.
// docs/design-table-exceptions.md describes the table model, docs/design-cl-exceptions.md cl's own model (row M7).
//
// Strict C++98.
#ifndef CPPPARSE_EHMODEL_HPP
#define CPPPARSE_EHMODEL_HPP

namespace cppparse {

// EhModelSjlj: try regions as setjmp frames (--eh=sjlj). EhModelTable: as unwind range tables with landing pads, no
// code on the normal path (--eh=table, the default). EhModelCl: cl's own model (--eh=cl): _CxxThrowException, FuncInfo
// tables and funclets for __CxxFrameHandler3, so that exceptions cross cl-compiled frames; its regions are the table
// model's
enum EhModel { EhModelSjlj, EhModelTable, EhModelCl };

inline const char* ehModelName(EhModel m) { return m == EhModelSjlj ? "sjlj" : m == EhModelTable ? "table" : "cl"; }

} // namespace cppparse

#endif
