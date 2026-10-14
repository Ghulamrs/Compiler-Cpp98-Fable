// driver-layout.cpp - compiled by cl and linked with cppparse's layout.obj: cl's value of every expression in
// layout.hpp's LAYOUT_VALUES must equal cppparse's, and objects laid out by cppparse must read back right
// through cl's view of the classes.
#define _CRT_SECURE_NO_WARNINGS
#include "layout.hpp"

#include <new>
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

// replace the library's operator new and delete for the whole program (layout.cpp replaces new[] and delete[])
long g_gNew = 0, g_gDel = 0;
void* operator new(size_t n) { g_gNew += (long)n; return malloc(n); }
void operator delete(void* p) { if (p) ++g_gDel; free(p); }

static void expect(const char* what, long got, long want)
{
    if (got == want) printf("  ok   %-40s %ld\n", what, got);
    else { printf("  FAIL %-40s %ld, expected %ld\n", what, got, want); ++failures; }
}

// the same calls as ovCall and owCall, made by cl: through cl's slots of cppparse's tables
static int clOvCall(const OV* p, int which)
{
    switch (which) {
    case 0: return p->a();
    case 1: return p->f(1);
    case 2: return p->b();
    case 3: return p->f(1.0);
    case 4: return p->c();
    case 5: return p->f('x');
    }
    return -1;
}

static int clOwCall(const OW* p, int which)
{
    switch (which) {
    case 6: return p->g();
    case 7: return p->f(1L);
    case 8: return p->g(1);
    case 9: return p->f((short)1);
    }
    return clOvCall(p, which);
}

// a class cl derives from cppparse's RC with covariant overrides of its own: cl's return-adjusting thunks in cl's
// copies of the tables, reached by cppparse's calls through RB* and RC*
struct RD : RC { RD* self() { return this; } RD* six(int p, int q, int r, int s, double t, int u) { return p + q + r + s + (int)t + u == 21 ? this : 0; } };

// a class cl derives from cppparse's OW: cppparse's calls reach its overriders through cl's table
struct CW : OW {
    int f(int) const { return 201; }
    int f(short) const { return 202; }
    int g() const { return 203; }
    int b() const { return 204; }
};

int main()
{
    const long cl[] = {
#define LAYOUT_ENTRY(e) (long)(e),
        LAYOUT_VALUES(LAYOUT_ENTRY)
#undef LAYOUT_ENTRY
    };
    const char* const names[] = {
#define LAYOUT_ENTRY(e) #e,
        LAYOUT_VALUES(LAYOUT_ENTRY)
#undef LAYOUT_ENTRY
    };
    printf("layouts: cppparse's value, compared with cl's\n");
    for (int k = 0; k < (int)(sizeof(cl) / sizeof(cl[0])); ++k) expect(names[k], layoutValue(k), cl[k]);
    expect("layoutValue past the end", layoutValue((int)(sizeof(cl) / sizeof(cl[0]))), -1);

    printf("objects shared with cppparse\n");
    expect("g_em.w", g_em.w, 41);
    expect("g_ne.n", g_ne.n, 7);
    expect("g_ne.w", g_ne.w, 9);
    EM em; em.w = 5;
    expect("emSum(&em)", emSum(&em), 6);
    EVal v;
    expect("evalGet(v, 12)", evalGet(v, 12), 12);
    expect("v.v after evalGet", v.v, 12);

    printf("bit-fields and anonymous members\n");
    expect("g_bf.a", g_bf.a, 5);
    expect("g_bf.b", g_bf.b, 17);
    expect("g_bf.c", g_bf.c, 0xABCDEF);
    expect("g_bf.d", g_bf.d, 'd');
    expect("g_bf6.a", g_bf6.a, -3);
    expect("g_bf6.b", g_bf6.b, 300);
    expect("g_bf6.c", g_bf6.c, -1);
    expect("g_bf6.s", g_bf6.s, -16);
    expect("g_bf7.a == -2", g_bf7.a == -2, 1);
    expect("g_bf7.b", g_bf7.b, 3);
    expect("g_bf7.c", (long)g_bf7.c, 0x2345678);
    expect("g_bf8.f", g_bf8.f, 1);
    expect("g_bf8.g", g_bf8.g, 0);
    expect("g_bf8.h", g_bf8.h, 'h');
    expect("g_bf8.k", g_bf8.k, Red);
    expect("g_bfp.c", g_bfp.c, 'p');
    expect("g_bfp.a", g_bfp.a, 7);
    expect("g_bfp.b", g_bfp.b, -100000);
    expect("g_agg.x", g_agg.x, 11);
    expect("g_agg.y", g_agg.y, 9);
    expect("g_agg.z", g_agg.z, -7);
    expect("g_au.k", g_au.k, 1);
    expect("g_au.dd * 2", (long)(g_au.dd * 2), 5);
    expect("g_au.z", g_au.z, 'z');
    expect("g_an.t", g_an.t, 't');
    expect("g_an.all", g_an.all, 0x56781234);
    expect("g_an.f", g_an.f, 9);
    expect("g_an.gc & 7", g_an.gc & 7, 5);
    BF2 b2;
    fillBits(&b2, -1, 5, 1);
    expect("BF2 a written by cppparse", b2.a, -1);
    expect("BF2 b written by cppparse", b2.b, 5);
    expect("BF2 c written by cppparse", b2.c, 1);
    b2.a = 3; b2.b = -8; b2.c = -2;
    expect("BF2 a read by cppparse", readBits(&b2, 0), 3);
    expect("BF2 b read by cppparse", readBits(&b2, 1), -8);
    expect("BF2 c read by cppparse", readBits(&b2, 2), -2);
    BF6 b6; b6.a = -3; b6.b = 300; b6.c = -1; b6.s = -16;
    expect("bitOps", bitOps(&b6), 1111111);
    expect("b6.a after bitOps", b6.a, -3);
    expect("b6.b after bitOps", b6.b, 511);
    expect("b6.s after bitOps", b6.s, -15);
    BF7 b7; b7.a = -2; b7.b = -4; b7.c = -5;
    expect("BF7 a read by cppparse", readBf7(&b7, 0), 1);
    expect("BF7 b read by cppparse", readBf7(&b7, 1), -4);
    expect("BF7 c read by cppparse", readBf7(&b7, 2), -5);

    printf("polymorphic bases\n");
    DP dp; DQ dq; DR dr;
    const PB& pdp = dp; const PB& pdq = dq; const PB& pdr = dr;
    expect("dp.x, dp.y, dp.z", dp.x * 100 + dp.y * 10 + dp.z, 123);
    expect("dp f() through cl's PB&", pdp.f(), 123);
    expect("dq f() through cl's PB&", pdq.f(), 5);
    expect("dr f() through cl's PB&", pdr.f(), 2004);
    expect("dq g() called by cl", dq.g(), 4 + 5 + 'q');
    expect("dr g() through cl's DQ&", static_cast<const DQ&>(dr).g(), 1007);
    expect("polyCall(&dp)", polyCall(&dp), 123);
    expect("polyCall(&dr)", polyCall(&dr), 2004);
    expect("polyCallG(dr)", polyCallG(dr), 1007);
    expect("polyX(&dp)", polyX(&dp), 1);
    expect("polyX(&dr)", polyX(&dr), 4);
    expect("dr.r", dr.r, 7);

    printf("two polymorphic bases: secondary vftables and thunks\n");
    {
        SD d; SE e;
        const SA& ra = d; const SB& rb = d;
        expect("saCall(&d, a)", saCall(&d, 0), 1);
        expect("saCall(&d, f)", saCall(&d, 1), 130);
        expect("sbCall(&d, b)", sbCall(&d, 0), 310);
        expect("sbCall(&d, f) (thunk)", sbCall(&d, 1), 130);
        expect("sbCall(&d, g)", sbCall(&d, 2), 260);
        expect("sdCall(&d, f)", sdCall(&d, 0), 130);
        expect("sdCall(&d, g)", sdCall(&d, 1), 260);
        expect("sdCall(&d, b)", sdCall(&d, 2), 310);
        expect("sdCall(&d, h)", sdCall(&d, 3), 660);
        expect("sdCall(&d, a)", sdCall(&d, 4), 1);
        expect("sdCall(&d, SD::g)", sdCall(&d, 5), 260);
        expect("d f() through cl's SA&", ra.f(), 130);
        expect("d f() through cl's SB&", rb.f(), 130);
        expect("d g() through cl's SB&", rb.g(), 260);
        expect("d b() through cl's SB&", rb.b(), 310);
        expect("d.g() called by cl", d.g(), 260);
        expect("d.SD::g() called by cl", d.SD::g(), 260);
        expect("d.h() called by cl", d.h(), 660);
        expect("sbCall(&e, g)", sbCall(&e, 2), 601);
        expect("sbCall(&e, f)", sbCall(&e, 1), 130);
        expect("saCall(&e, a)", saCall(&e, 0), 607);
        expect("sdCall(&e, g)", sdCall(&e, 1), 601);
        expect("sdCall(&e, h)", sdCall(&e, 3), 1001);
        expect("e g() through cl's SB&", static_cast<const SB&>(e).g(), 601);
        expect("e.a() called by cl", e.a(), 607);
        expect("e.sb, e.sd, e.x, e.se", e.sb + e.sd + e.x + e.se, 20 + 30 + 7 + 'e');
        expect("destroyThroughSB(SD)", destroyThroughSB(0), 12);
        expect("destroyThroughSB(SE)", destroyThroughSB(1), 312);
        static double storage[8];
        SB* pb = new (storage) SE;
        g_dtorLog = 0;
        pb->~SB();
        expect("SE destroyed by cl through SB*", g_dtorLog, 312);
        expect("deleteThroughSB(SD)", deleteThroughSB(0), 12);
        expect("deleteThroughSB(SE)", deleteThroughSB(1), 312);
        SB* q = new SE;
        g_dtorLog = 0;
        delete q;                          // cl calls the slot's deleting destructor with flags 1
        expect("SE deleted by cl through SB*", g_dtorLog, 312);
        SD* r = new SD;
        g_dtorLog = 0;
        delete r;
        expect("SD deleted by cl through SD*", g_dtorLog, 12);

        // virtual calls inside constructors and destructors reach the class being built or destroyed
        g_sbCtorSaw = g_sbDtorSaw = g_sdDtorSaw = 0;
        SB* s = new SE;
        expect("g() in SB() of an SE built by cl", g_sbCtorSaw, 5);
        delete s;
        expect("g() in ~SB() of an SE deleted by cl through SB*", g_sbDtorSaw, 5);
        expect("h() in ~SD() of an SE deleted by cl through SB*", g_sdDtorSaw, 660);
        g_sbCtorSaw = g_sbDtorSaw = g_sdDtorSaw = 0;
        SE* t = new SE;
        delete t;
        expect("g() in ~SB() of an SE deleted by cl through SE*", g_sbDtorSaw, 5);
        expect("h() in ~SD() of an SE deleted by cl through SE*", g_sdDtorSaw, 660);
        g_sbCtorSaw = g_sbDtorSaw = g_sdDtorSaw = 0;
        expect("deleteThroughSB(SE) again", deleteThroughSB(1), 312);
        expect("g() in SB() of an SE built by cppparse", g_sbCtorSaw, 5);
        expect("g() in ~SB() of an SE deleted by cppparse", g_sbDtorSaw, 5);
        expect("h() in ~SD() of an SE deleted by cppparse", g_sdDtorSaw, 660);

        // the deleting destructor frees through the class's operator delete, with the complete object's size
        g_kDeleted = 0;
        deleteKB(makeKDasKB());
        expect("KB::operator delete size, delete by cppparse", g_kDeleted, (long)sizeof(KD));
        g_kDeleted = 0;
        KB* kb = makeKDasKB();
        delete kb;
        expect("KB::operator delete size, delete by cl", g_kDeleted, (long)sizeof(KD));

        // two tables introduced by the same class get distinct names (built and called by both compilers)
        GG* gg = makeGG();
        expect("ggCall(gg)", ggCall(gg), 18 * 100 + 98);
        expect("gg fp() through cl's GQ1*", static_cast<GQ1*>(gg)->fp(), 18);
        expect("gg fq() through cl's GQ2*", static_cast<GQ2*>(gg)->fq(), 98);
        delete static_cast<GQ2*>(gg);
    }
    {
        // virtual bases: objects built by one compiler, converted, copied and destroyed by the other
        g_vbLog = 0;
        VX x;
        expect("VX built by cl: log", g_vbLog, 1234);
        expect("v through VL* (cppparse)", vbReadV(&x), 7);
        expect("v and r through VR& (cppparse)", vbReadVR(x), 73);
        expect("VX* to VB* (cppparse), null", vbToVB(0) != 0, 0);
        expect("VX* to VB* (cppparse)", (long)((char*)vbToVB(&x) - (char*)&x), (long)((char*)static_cast<VB*>(&x) - (char*)&x));
        VM m;
        expect("VM read by cppparse", vbReadVM(&m), 5 + 7 + 5);
        VN n;
        expect("VN read by cppparse", vbReadVN(n), 79);
        g_vbLog = 0;
        VX* p = vbMakeVX();
        expect("VX built by cppparse: log", g_vbLog, 1234);
        expect("v of cppparse's VX through cl's VL*", static_cast<VL*>(p)->v, 7);
        expect("r of cppparse's VX through cl's VR*", static_cast<VR*>(p)->r + static_cast<VR*>(p)->v, 10);
        g_vbLog = 0;
        vbDelete(p);
        expect("VX deleted by cppparse: log", g_vbLog, 69);
        p = new VX;
        g_vbLog = 0;
        vbDelete(p);
        expect("cl's VX deleted by cppparse: log", g_vbLog, 69);
        p = vbMakeVX();
        g_vbLog = 0;
        delete p;
        expect("cppparse's VX deleted by cl: log", g_vbLog, 69);
        expect("VX on cppparse's stack: log", vbOnStack(), 123469);
        expect("VX copied by cppparse", vbCopy(x), 107234);
        {
            VX c = x;
            expect("cl's copy read by cppparse", vbReadV(&c) * 10 + vbReadVR(c) % 10, 1073);
        }
        expect("new VX[2], delete[] by cppparse", vbArray(2), 16969);
        VY y;
        expect("VY (VB named by the most derived class)", vbSumVY(y), 7 + 2 + 3 + 4 + 6);
        VZ z;
        expect("VZ (vbptrs shared from VX at 8)", vbSumVZ(z), 7384);
        VD d;
        expect("VD v d", d.v * 10 + d.d, 71);
        g_vbLog = 0;
        {
            VNF f;
            expect("VNF built by cl: log", g_vbLog, 1235);
            expect("VNF read by cppparse", vbNestedF(f), 12356);
            VNG g;
            expect("VNG read by cppparse", vbNestedG(&g), 12358);
            expect("VNG* to VNA* (cppparse)", (long)((char*)vbToVNA(&g) - (char*)&g), (long)((char*)static_cast<VNA*>(&g) - (char*)&g));
            g_vbLog = 0;
        }
        expect("VNF and VNG destroyed by cl: log", g_vbLog, 7979);
        expect("VNF and VNG on cppparse's stack: log", vbNestedLife(), 17979);
        g_vbLog = 0;
    }

    printf("vftable slots of overloads\n");
    {
        static const long wantV[6] = { 1, 2, 3, 4, 5, 6 };
        static const long wantW[10] = { 111, 12, 13, 114, 15, 16, 117, 118, 119, 120 };
        static const long wantC[10] = { 111, 201, 204, 114, 15, 16, 203, 118, 119, 202 };
        OV ov; OW ow; CW cw;
        char what[64];
        for (int k = 0; k < 6; ++k) {
            sprintf(what, "ovCall(&ov, %d)", k); expect(what, ovCall(&ov, k), wantV[k]);
            sprintf(what, "cl's ovCall(&ov, %d)", k); expect(what, clOvCall(&ov, k), wantV[k]);
            sprintf(what, "ovCall(&ow, %d)", k); expect(what, ovCall(&ow, k), wantW[k]);
            sprintf(what, "cl's ovCall(&ow, %d)", k); expect(what, clOvCall(&ow, k), wantW[k]);
        }
        for (int k = 0; k < 10; ++k) {
            sprintf(what, "owCall(&ow, %d)", k); expect(what, owCall(&ow, k), wantW[k]);
            sprintf(what, "cl's owCall(&ow, %d)", k); expect(what, clOwCall(&ow, k), wantW[k]);
            sprintf(what, "owCall(&cw, %d) (cl's class)", k); expect(what, owCall(&cw, k), wantC[k]);
            sprintf(what, "cl's owCall(&cw, %d)", k); expect(what, clOwCall(&cw, k), wantC[k]);
        }
        g_ovLog = 0;
        delete makeOW();
        expect("OW deleted by cl through OV*", g_ovLog, 21);
        g_ovLog = 0;
        deleteOV(makeOW());
        expect("OW deleted by cppparse through OV*", g_ovLog, 21);
        g_ovLog = 0;
        deleteOV(new CW);
        expect("CW (cl's) deleted by cppparse through OV*", g_ovLog, 21);
    }

    printf("arrays of a polymorphic class: the vector deleting destructor\n");
    {
        const long avSize = (long)sizeof(AV), awSize = (long)sizeof(AW);
        g_avNext = 0; g_avLog = 0; g_avFreed = 0;
        AV* p = makeAVArray(3);
        expect("AV array built by cppparse, v()", p[0].v() + p[1].v() * 10 + p[2].v() * 100, 321);
        delete[] p;
        expect("AV array of cppparse deleted by cl: order", g_avLog, 321);
        expect("AV array of cppparse deleted by cl: operator delete[] size", g_avFreed, 3 * avSize + 8);
        g_avNext = 0; g_avLog = 0; g_avFreed = 0;
        p = new AV[3];
        deleteAVArray(p);
        expect("AV array of cl deleted by cppparse: order", g_avLog, 321);
        expect("AV array of cl deleted by cppparse: operator delete[] size", g_avFreed, 3 * avSize + 8);
        g_avNext = 0; g_avLog = 0; g_avFreed = 0;
        AW* q = makeAWArray(2);
        delete[] q;
        expect("AW array of cppparse deleted by cl: order", g_avLog, 7261);
        expect("AW array of cppparse deleted by cl: operator delete[] size", g_avFreed, 2 * awSize + 8);
        g_avNext = 0; g_avLog = 0; g_avFreed = 0;
        q = new AW[2];
        deleteAWArray(q);
        expect("AW array of cl deleted by cppparse: order", g_avLog, 7261);
        expect("AW array of cl deleted by cppparse: operator delete[] size", g_avFreed, 2 * awSize + 8);
        g_avNext = 0; g_avLog = 0; g_avFreed = 0;
        q = makeAWArray(2);
        deleteAWArray(q);
        expect("AW array of cppparse deleted by cppparse: order", g_avLog, 7261);
        expect("AW array of cppparse deleted by cppparse: size", g_avFreed, 2 * awSize + 8);
        g_avNext = 0; g_avLog = 0; g_avFreed = 0;
        p = makeAVArray(0);
        delete[] p;
        expect("empty AV array of cppparse deleted by cl: size", g_avFreed, 8);
        g_avFreed = 0;
        deleteAVArray(new AV[0]);
        expect("empty AV array of cl deleted by cppparse: size", g_avFreed, 8);
        g_avNext = 0; g_avLog = 0;
        AV* one = new AW;
        delete one;                        // the same slot with flags 1: one object, operator delete
        expect("AW deleted by cl through AV* (flags 1)", g_avLog, 61);
    }

    printf("class-specific operator new, new[], delete, delete[]\n");
    {
        const long tn = (long)sizeof(TN), tm = (long)sizeof(TM);
        g_tnNew = g_tnDel = 0;
        TN* a = makeTN();
        expect("TN::operator new size, new by cppparse", g_tnNew, tn);
        delete a;
        expect("TN::operator delete size, delete by cl", g_tnDel, tn);
        g_tnNew = g_tnDel = 0;
        deleteTN(new TN);
        expect("TN::operator new size, new by cl", g_tnNew, tn);
        expect("TN::operator delete size, delete by cppparse", g_tnDel, tn);
        g_tnNew = g_tnDel = 0; g_tnNext = 0; g_tnLog = 0;
        a = makeTNArray(3);
        expect("TN::operator new[] size, new[] by cppparse", g_tnNew, 3 * tn + 8);
        delete[] a;
        expect("TN array of cppparse deleted by cl: order", g_tnLog, 321);
        expect("TN::operator delete[] size, delete[] by cl", g_tnDel, 3 * tn + 8);
        g_tnNew = g_tnDel = 0; g_tnNext = 0; g_tnLog = 0;
        deleteTNArray(new TN[3]);
        expect("TN::operator new[] size, new[] by cl", g_tnNew, 3 * tn + 8);
        expect("TN array of cl deleted by cppparse: order", g_tnLog, 321);
        expect("TN::operator delete[] size, delete[] by cppparse", g_tnDel, 3 * tn + 8);
        g_tnNew = g_tnDel = 0; g_tnNext = 0; g_tnLog = 0;
        TM* b = makeTMArray(2);
        expect("inherited operator new[] size, new[] by cppparse", g_tnNew, 2 * tm + 8);
        delete[] b;
        expect("TM array of cppparse deleted by cl: order", g_tnLog, 7261);
        expect("inherited operator delete[] size, delete[] by cl", g_tnDel, 2 * tm + 8);
        g_tnNew = g_tnDel = 0; g_tnNext = 0; g_tnLog = 0;
        deleteTMArray(new TM[2]);
        expect("inherited operator new[] size, new[] by cl", g_tnNew, 2 * tm + 8);
        expect("TM array of cl deleted by cppparse: order", g_tnLog, 7261);
        expect("inherited operator delete[] size, delete[] by cppparse", g_tnDel, 2 * tm + 8);
        g_tnNew = g_tnDel = 0;
        deleteTNGlobal(makeTNGlobal());
        expect("::new and ::delete bypass TN's operators", g_tnNew + g_tnDel, 0);
        g_trNew = g_trDel = 0;
        TR* r = makeTRArray(3);
        expect("TR::operator new[] size (no cookie), by cppparse", g_trNew, 3 * (long)sizeof(TR));
        delete[] r;
        expect("TR::operator delete[] (no cookie), delete[] by cl", g_trDel, 1);
        g_trDel = 0;
        deleteTRArray(new TR[3]);
        expect("TR::operator delete[] (no cookie), delete[] by cppparse", g_trDel, 1);
        g_trDel = 0;
        TQ* q = makeTQArray(2);
        delete[] q;
        expect("TQ::operator delete[] size (no cookie), by cl", g_trDel, (long)sizeof(TQ));
        g_trDel = 0;
        deleteTQArray(new TQ[2]);
        expect("TQ::operator delete[] size (no cookie), by cppparse", g_trDel, (long)sizeof(TQ));
        g_trDel = 0;
        deleteTS(new TS);
        expect("TS::operator delete, delete by cppparse", g_trDel, 1);
    }
    printf("replaced global operator new and delete (cl's unit), new[] and delete[] (cppparse's unit)\n");
    {
        g_gNew = g_gDel = g_gNewA = g_gDelA = 0;
        int* i = makeInt(7);
        delete i;
        deleteInt(new int(8));
        expect("operator new bytes: new int by cppparse, by cl", g_gNew, 8);
        expect("operator delete calls: delete by cl, by cppparse", g_gDel, 2);
        int* is = makeInts(5);
        delete[] is;
        deleteInts(new int[5]);
        expect("operator new[] bytes: new int[5] by both", g_gNewA, 40);
        expect("operator delete[] calls: delete[] by both", g_gDelA, 2);
        g_gNew = g_gDel = g_gNewA = g_gDelA = 0;
        GQ* g = makeGQ();
        delete g;
        deleteGQ(new GQ);
        expect("operator new bytes: new GQ by both", g_gNew, 2 * (long)sizeof(GQ));
        expect("operator delete calls: GQ's deleting destructor", g_gDel, 2);
        GC* c = makeGCArray(3);
        delete[] c;
        deleteGCArray(new GC[3]);
        expect("operator new[] bytes: new GC[3] by both", g_gNewA, 2 * (3 * (long)sizeof(GC) + 8));
        expect("operator delete[] calls: delete[] of GC by both", g_gDelA, 2);
        g_gNewA = g_gDelA = 0;
        TQ* tq = makeTQArray(2);
        expect("operator new[] bytes: new TQ[2] by cppparse", g_gNewA, 2 * (long)sizeof(TQ));
        deleteTQArray(tq);
        expect("TQ's delete[], not the global one", g_gDelA, 0);
    }
    printf("review fixes: grouping with a non-virtual name, hidden overrides, both operator delete[] forms\n");
    {
        ZG z;
        const long wantZ[4] = { 1, 2, 3, 4 };
        char what[64];
        for (int k = 0; k < 4; ++k) {
            sprintf(what, "zgCall(&z, %d)", k); expect(what, zgCall(&z, k), wantZ[k]);
        }
        expect("cl's z.g()", static_cast<const ZG&>(z).g(), 1);
        expect("cl's z.f(int)", static_cast<const ZG&>(z).f(1), 2);
        expect("cl's z.f(double)", static_cast<const ZG&>(z).f(1.0), 3);
        YA* ya = makeYC();
        expect("yaCall(YC, f(int))", yaCall(ya, 0), 31);
        expect("yaCall(YC, f(double))", yaCall(ya, 1), 22);
        expect("cl's YA* f(int) of cppparse's YC", ya->f(1), 31);
        expect("cl's YA* f(double) of cppparse's YC", ya->f(1.0), 22);
        expect("cl's YC* k()", static_cast<YC*>(ya)->k(), 33);
        YC yc;
        expect("ycCall(cl's YC, f(int))", ycCall(&yc, 0), 31);
        expect("ycCall(cl's YC, k())", ycCall(&yc, 1), 33);
        expect("yaCall(cl's YC, f(int))", yaCall(&yc, 0), 31);
        delete static_cast<YC*>(ya);
        g_dnForm = 0;
        delete[] makeDNArray(2);
        expect("DN array of cppparse deleted by cl: operator delete[](void*)", g_dnForm, 1);
        g_dnForm = 0;
        deleteDNArray(new DN[2]);
        expect("DN array of cl deleted by cppparse: operator delete[](void*)", g_dnForm, 1);
    }
    printf("covariant returns through a secondary base: cl's return-adjusting thunks, by both compilers\n");
    {
        RC c;
        RB* asB = &c;
        expect("cppparse's call through RB* (the thunk)", rbSelf(&c) == &c, 1);
        expect("cppparse's call through RC* (the new slot)", rcSelf(&c) == &c, 1);
        expect("cppparse's reference result through RB&", &rbSelfRef(c) == &c, 1);
        expect("cppparse's six through RB*, found", rbSix(&c, 6) == &c, 1);
        expect("cppparse's six through RB*, null", rbSix(&c, 7) == 0, 1);
        expect("cppparse's six through RC*", rcSix(&c, 6) == &c, 1);
        expect("cl's call through RB* into cppparse's table", asB->self() == &c, 1);
        expect("cl's call through RC*", c.self() == &c, 1);
        expect("cl's reference result through RB&", &asB->selfRef() == &c, 1);
        expect("cl's six through RB*", asB->six(1, 2, 3, 4, 5.0, 6) == &c, 1);
        expect("cl's six through RB*, null", asB->six(1, 2, 3, 4, 5.0, 7) == 0, 1);
        // cl's class derived from cppparse's RC: cppparse's calls reach cl's thunks and real slots
        RD d;
        expect("cppparse's RB* call of cl's override", rbSelf(&d) == &d, 1);
        expect("cppparse's RC* call of cl's override", rcSelf(&d) == &d, 1);
        expect("cppparse's RB* six of cl's override", rbSix(&d, 6) == &d, 1);
        expect("cppparse's RC* six of cl's override, null", rcSix(&d, 7) == 0, 1);
        expect("RC::rc through cl", static_cast<const RC&>(d).rc(), 3);
    }
    printf("pointers to members: made, passed, applied, compared and stored by both compilers\n");
    {
        MP p; MQ q; VL vl;
        // cppparse's values read through cl's view of the representations
        expect("g_mpB (offset of b)", g_mpB == &MP::b, 1);
        expect("g_mpNull (-1)", *(const int*)&g_mpNull, -1);
        expect("g_mpF (the vcall thunk)", g_mpF == &MP::f, 1);
        expect("g_mqM (16 + m)", *(const int*)&g_mqM, 16 + (int)LAYOUT_FIELD(MM, m));
        expect("g_mqF == &MP::f as MQ's", g_mqF == &MP::f, 1);
        expect("g_mqMf adjustment", ((const int*)&g_mqMf)[2], 16);
        expect("g_vlV (offset, vbtable entry)", ((const int*)&g_vlV)[0] * 100 + ((const int*)&g_vlV)[1], 4);
        expect("g_vlGet == &VB::get as VL's", g_vlGet == &VB::get, 1);
        expect("g_mpArr: &MP::a, -1, -1", ((const int*)g_mpArr)[0] * 100 + ((const int*)g_mpArr)[1] + ((const int*)g_mpArr)[2], 8 * 100 - 2);
        expect("g_mh.field (-1 in static storage)", *(const int*)&g_mh.field, -1);
        expect("g_mh.field == 0", g_mh.field == 0, 1);
        // cl's values applied by cppparse
        expect("mpRead(&p, &MP::a)", mpRead(&p, &MP::a), 1);
        expect("mpRead(&p, &MP::b)", mpRead(&p, &MP::b), 2);
        expect("mpCall(&p, &MP::f) (virtual)", mpCall(&p, &MP::f), 10);
        expect("mpCall(&q, &MP::f) (MQ::f through the thunk)", mpCall(&q, &MP::f), 40);
        expect("mpCall(&p, &MP::h)", mpCall(&p, &MP::h), 20);
        expect("mpReadQ(q, &MM::m)", mpReadQ(q, &MM::m), 3);
        expect("mpReadQ(q, &MP::b)", mpReadQ(q, &MP::b), 2);
        expect("mpCallQ(q, &MM::mf)", mpCallQ(q, &MM::mf), 300);
        expect("mpCallQ(q, &MP::f)", mpCallQ(q, &MP::f), 40);
        expect("mpCallQ(q, &MP::h)", mpCallQ(q, &MP::h), 20);
        expect("mpReadVL(&vl, &VB::v)", mpReadVL(&vl, &VB::v), 7);
        expect("mpReadVL(&vl, &VL::l)", mpReadVL(&vl, &VL::l), 2);
        expect("mpCallVL(&vl, &VB::get)", mpCallVL(&vl, &VB::get), 1007);
        // cppparse's results applied by cl
        expect("p.*mpOfB()", p.*mpOfB(), 2);
        expect("(q.*mqOfMf())()", (q.*mqOfMf())(), 300);
        expect("vl.*vlOfV()", vl.*vlOfV(), 7);
        expect("(vl.*vlOfGet())()", (vl.*vlOfGet())(), 1007);
        expect("mpOfB() == &MP::b", mpOfB() == &MP::b, 1);
        expect("mqOfMf() == &MM::mf", mqOfMf() == &MM::mf, 1);
        expect("vlOfV() == &VB::v", vlOfV() == &VB::v, 1);
        // null and equality, cl's values compared by cppparse
        int (MQ::*nullMq)() const = 0;
        expect("mpIsNull(0, 0, 0, 0)", mpIsNull(0, 0, 0, 0), 15);
        expect("mpIsNull(&MP::a, nullMq, &VB::v, 0)", mpIsNull(&MP::a, nullMq, &VB::v, 0), 10);
        expect("mpIsNull(g_mpNull, &MP::f, 0, &VB::get)", mpIsNull(g_mpNull, &MP::f, 0, &VB::get), 5);
        expect("mpEqual(&MP::f, &MP::f, &VB::v, &VB::v)", mpEqual(&MP::f, &MP::f, &VB::v, &VB::v), 3);
        expect("mpEqual(&MP::f, &MM::mf, &VB::v, &VL::l)", mpEqual(&MP::f, &MM::mf, &VB::v, &VL::l), 0);
        expect("mpEqual(&MP::f, &MP::h, 0, 0)", mpEqual(&MP::f, &MP::h, 0, 0), 2);
        // cl's functions called by cppparse with cppparse's values
        expect("mpViaCl(&p, q)", mpViaCl(&p, q), 302);
        // cl's derived class: cppparse's thunk reaches its override
        struct MR : MP { int f() const { return 77; } } mr;
        expect("mpCall(&mr, &MP::f) (cl's override)", mpCall(&mr, &MP::f), 77);
        expect("mpCall(&mr, g_mpF)", mpCall(&mr, g_mpF), 77);
    }
    if (failures) { printf("%d FAILED\n", failures); return 1; }
    printf("ALL OK\n");
    return 0;
}

int clMpRead(const MP* p, int MP::* m) { return p->*m; }
int clMpCallQ(const MQ& q, int (MQ::*f)() const) { return (q.*f)(); }
