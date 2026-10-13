// ambiguous-base.cpp - must be rejected (S141, cl C2584): a direct base that is also a base of another direct
// base, one occurrence virtual and the other not, with the direct one listed first; the forms cl only warns about
// (C4584) are warned about (S142), and the one cl accepts silently is accepted
struct V { int v; };
struct W : V { };
struct WV : virtual V { };
struct X : W { };
struct F : virtual V, W { };     // C2584
struct H2 : V, WV { };           // C2584
struct H5 : virtual V, X { };    // C2584
struct G2 : V, W { };            // C4584
struct H3 : WV, V { };           // C4584
struct F2 : W, virtual V { };    // accepted
struct H1 : virtual V, WV { };   // accepted (one shared V)

int main() { return 0; }
