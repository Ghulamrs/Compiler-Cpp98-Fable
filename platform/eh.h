// eh.h - the C runtime's terminate state as cppparse's own runtime uses it (row P1 stage 2, docs/design-selfhost.md
// 4.3): one terminate handler for the program, the C++ runtime's underneath (vcruntime's, grtcxx's), with C linkage
// as the C runtime defines them.
#ifndef CPPPARSE_EH_H
#define CPPPARSE_EH_H

#include <cstddef>

extern "C" {
    typedef void (*terminate_handler)(void);
    __cppparse_noreturn void terminate(void);
    terminate_handler        set_terminate(terminate_handler handler);
    bool                     __uncaught_exception(void);
}

#endif
