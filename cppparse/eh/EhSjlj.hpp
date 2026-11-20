// EhSjlj.hpp - the setjmp model (--eh=sjlj, version 5): one frame per try region in the function's stack frame.
//
//     lea rcx, [frame]  ; call cppparse_push_frame
//     lea rcx, [frame]  ; xor edx, edx ; call _setjmp      (frame pointer NULL: no unwinding)
//     test eax, eax     ; jne dispatch
//     ... try body ...
//     lea rcx, [frame]  ; call cppparse_pop_frame           (also before return/break/continue)
//   dispatch:
//     lea rcx, [frame]  ; call cppparse_pop_frame
//     ... the clauses (EhPads) ...
//
// Strict C++98.
#ifndef CPPPARSE_EHSJLJ_HPP
#define CPPPARSE_EHSJLJ_HPP

#include "EhEmitter.hpp"

namespace cppparse {

class EhSjlj {
public:
    enum { FrameSize = 272 };   // sizeof(jmp_buf) on x64 (256) plus the frame link; 16-byte aligned in the stack frame

    // a region begins: register the frame, then _setjmp(frame, NULL): 0 now, 1 when a throw lands here, which goes
    // to the region's dispatch stub
    static void tryBegin(EhEmitter& out, const std::string& frame, const std::string& dispatchLabel);
    // a region ends on the normal path
    static void tryEnd(EhEmitter& out, const std::string& frame);
    // the head of a dispatch stub: the frame the throw landed in is popped
    static void dispatchPop(EhEmitter& out, const std::string& frame);
};

} // namespace cppparse

#endif
