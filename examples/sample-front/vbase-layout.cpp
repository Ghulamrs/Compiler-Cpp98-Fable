// vbase-layout.cpp - MSVC's layout of classes with virtual bases, checked by --sema6 against cl
// (/d1reportSingleClassLayout): the vbptr (own, after the non-virtual bases with the members moved down, or
// shared with the first non-virtual base that has one), each virtual base once after the non-virtual part, the
// vbtables (??_8) with their entries and the bases that name them apart. --compile6 still refuses these (S130).
struct VB { int v; VB() : v(7) {} ~VB() {} };
struct VD : virtual VB { int d; VD() : d(1) {} };
struct VL : virtual VB { int l; };
struct VR : virtual VB { int r; };
struct VX : VL, VR { int x; };
struct NB { char c; };
struct VN : NB, virtual VB { short n; };
struct V2 { double q; };
struct VM : virtual VB, NB, virtual V2 { int m; };
struct VY : VN, VD { int y; };
struct VZ : NB, VX { int z; };
struct VW : VD, VX { int w; };
// virtual bases with virtual bases of their own: their vbptrs in the virtual part, tables named by the bases
struct VNA { int a; VNA() : a(1) {} };
struct VNB : virtual VNA { int b; VNB() : b(2) {} };
struct VNC : virtual VNB { int c; VNC() : c(3) {} };
struct VND : VNC { int d; VND() : d(4) {} };
struct VNE : virtual VNB, virtual VNA { int e; };
struct VNF : VNC, VNE { int f; };
struct VNG : virtual VNC, virtual VNE { int g; };

VD gd; VX gx; VN gn; VM gm; VY gy; VZ gz; VW gw; VNC gnc; VND gnd; VNE gne; VNF gnf; VNG gng;

int main() { return 0; }
