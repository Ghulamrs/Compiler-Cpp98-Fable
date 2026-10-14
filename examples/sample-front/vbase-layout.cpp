// vbase-layout.cpp - MSVC's layout of classes with virtual bases, checked by --sema6 against cl
// (/d1reportAllClassLayout): the vbptr (own, after the non-virtual bases with the members moved down, or
// shared with the first non-virtual base that has one), each virtual base once after the non-virtual part, the
// vbtables (??_8) with their entries and the bases that name them apart, cl's vtordisp fields before polymorphic
// virtual bases.
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
// vbtables named by a path of several bases (R's four A subobjects: ??_8VPR@@7BVPB@@VPP@@@)
struct VPA : virtual VNA { int a; };
struct VPB : VPA { int b; };
struct VPC : VPA { int c; };
struct VPP : VPB, VPC { int p; };
struct VPQ : VPB, VPC { int q; };
struct VPR : VPP, VPQ { int r; };
// polymorphic virtual bases: the vfptr stays in the virtual base, a class gets its own only for new virtual
// functions (VPN), and cl's vtordisp (4 bytes before the base) comes with a user-declared constructor or destructor
// and an override of the base's function (VPL, VPD, VPX, VPW), inherited by the classes derived from one (VPL2)
struct VPV { int v; VPV() : v(7) {} virtual int f() const { return v; } virtual int g() const { return 1; } virtual ~VPV() {} };
struct VPO : virtual VPV { int o; int f() const { return o; } };
struct VPO2 : VPO { int o2; };
struct VPO3 : VPO { int o3; VPO3() : o3(3) {} };
struct VPL : virtual VPV { int l; VPL() : l(2) {} int f() const { return l; } };
struct VPL2 : VPL { int l2; VPL2() : l2(5) {} };
struct VPRR : virtual VPV { int r; };
struct VPD : VPL, VPRR { int d; VPD() : d(4) {} int g() const { return d; } };
struct VPNB { int nb; VPNB() : nb(4) {} virtual int nf() const { return nb; } };
struct VPX : VPNB, virtual VPV { int mx; VPX() : mx(6) {} int f() const { return mx; } int nf() const { return mx + 1; } };
struct VPY : VPNB, virtual VPV { int my; virtual int h() const { return 9; } int f() const { return my; } };
struct VPN : virtual VPV { int n; virtual int h() const { return 3; } };
struct VPN2 : VPN { int n2; VPN2() : n2(8) {} int h() const { return n2; } int g() const { return n2 + 1; } };
struct VPZ : virtual VPV { int z; ~VPZ() {} };
struct VPW : virtual VPV { int w; VPW() : w(1) {} ~VPW() {} int f() const { return w; } };
struct VPWX : VPW { int wx; };
struct VPM2 : NB, VPL { int m2; };
struct VPM3 : VPL, NB { int m3; };
struct VPWL : virtual VPL { int wl; };
struct VPWD : virtual VPD { int wd; VPWD() : wd(1) {} int f() const { return wd; } };
struct VPE {};
struct VPVE : virtual VPE { int ve; };
struct VPEE : virtual VPE, virtual VPV { char e; };
struct VPA16 { __declspec(align(16)) int a; };
struct VPVA : virtual VPA16 { int va; };
#pragma pack(push, 2)
struct VPPK : virtual VPV { short pk; };
#pragma pack(pop)

VD gd; VX gx; VN gn; VM gm; VY gy; VZ gz; VW gw; VNC gnc; VND gnd; VNE gne; VNF gnf; VNG gng; VPR gpr;
VPO2 gpo2; VPO3 gpo3; VPL2 gpl2; VPD gpd; VPX gpx; VPY gpy; VPN2 gpn2; VPZ gpz; VPWX gpwx; VPM2 gpm2; VPM3 gpm3; VPWL gpwl; VPWD gpwd;
VPVE gpve; VPEE gpee; VPVA gpva; VPPK gppk;

int main() { return 0; }
