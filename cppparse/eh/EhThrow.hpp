// EhThrow.hpp - a throw's call of the runtime: cppparse's (the setjmp and table models) or cl's (--eh=cl).
//
//     cppparse_throw(object address, type descriptor, size)             a scalar: the runtime copies it
//     cppparse_throw_owned(object, type descriptor, size, destructor)   a class object the generated code has
//                                                                       copy-constructed into malloc storage
//     cppparse_rethrow()                                                "throw;"
// The type descriptor is a string ("Class;Base1;Base2@16", IrGen::throwDescriptorFor). The code generator
// materialises the operands; this class knows the registers the runtime takes them in and the entry to call.
//
// Strict C++98.
#ifndef CPPPARSE_EHTHROW_HPP
#define CPPPARSE_EHTHROW_HPP

#include "EhEmitter.hpp"

namespace cppparse {

class EhThrow {
public:
    static void rethrow(EhEmitter& out);
    // the object's address is in 'reg' (a class object, already in its heap storage)
    static void objectIn(EhEmitter& out, const std::string& reg);
    // a scalar in 'reg' goes to the frame's throw slot, whose address the runtime gets
    static void scalarIn(EhEmitter& out, const std::string& reg, bool isFloat, const std::string& slot);
    // the descriptor string and the size, then the call: of cppparse_throw, or (ownedDestructor set: the register
    // that holds the destructor's address or 0) of cppparse_throw_owned
    static void descriptor(EhEmitter& out, const std::string& typeDescriptor, int size);
    static void callOwned(EhEmitter& out, const std::string& destructorReg);
    static void call(EhEmitter& out);

    // cl's model: _CxxThrowException(the object, a temporary of the frame, its address in 'reg'; the ThrowInfo), and
    // with two nulls for "throw;"
    static void clThrow(EhEmitter& out, const std::string& reg, const std::string& throwInfoSymbol);
    static void clRethrow(EhEmitter& out);
};

} // namespace cppparse

#endif
