// wide-refused.cpp - must be rejected: an array bound beyond what cl allows (C2148: an array holds at most
// 0x7fffffff bytes), written with 64-bit constant expressions (front.cpp wide64:: has the valid ones)
typedef char TooBig[1LL << 32];
const long long kHuge = 1LL << 40;
int huge[kHuge];
