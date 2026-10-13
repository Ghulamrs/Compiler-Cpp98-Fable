// statics.cpp - compiled by cppparse: reaches the shared static locals of statics.hpp
#include "statics.hpp"

int cpp_touchA() { return sharedA().id; }
int cpp_touchB() { return sharedB().id; }
int cpp_touchC() { return sharedC(); }
const Counted* cpp_addrA() { return &sharedA(); }
const Counted* cpp_addrB() { return &sharedB(); }
