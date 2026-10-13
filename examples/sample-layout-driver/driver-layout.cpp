// driver-layout.cpp - compiled by cl and linked with cppparse's layout.obj: cl's value of every expression in
// layout.hpp's LAYOUT_VALUES must equal cppparse's, and objects laid out by cppparse must read back right
// through cl's view of the classes.
#define _CRT_SECURE_NO_WARNINGS
#include "layout.hpp"

#include <new>
#include <stdio.h>

static int failures = 0;

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
    }
    if (failures) { printf("%d FAILED\n", failures); return 1; }
    printf("ALL OK\n");
    return 0;
}
