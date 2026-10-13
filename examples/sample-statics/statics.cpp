// statics.cpp - compiled by cppparse: reaches the shared static locals of statics.hpp
#include "statics.hpp"

void cpp_konstK(KonstSeen& s) { konstK(s); }
int cpp_touchA() { return sharedA().id; }
int cpp_touchB() { return sharedB().id; }
int cpp_touchC() { return sharedC(); }
const Counted* cpp_addrA() { return &sharedA(); }
const Counted* cpp_addrB() { return &sharedB(); }
FlakyP::FlakyP(int w) : which(w) { if (++g_flakyAttempts[w] == 1) throw 7; ++g_flakyBuilt[w]; }
int cpp_flakyX() { try { return flakyX().which; } catch (int) { return -1; } }
int cpp_flakyY() { try { return flakyY().which; } catch (int) { return -1; } }
