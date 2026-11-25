// handler-order.cpp - a handler that an earlier handler of its try block hides (row G23). C++98 15.3/5 allows such a
// handler (it is never entered), so cppparse compiles the unit and warns (S162) on every marked line and nowhere
// else. cl /W4 warns on the C4286 lines and refuses the C2312, C2313 and C2315 lines; clang -std=c++98 warns on most
// of them. Compiled by test 17 at -O0; the object is not run.
struct B { virtual ~B() {} }; struct D : B {}; struct E : D {};
struct P : private B {}; struct A2 : B {}; struct A3 : B {}; struct M : A2, A3 {};
struct V { virtual ~V() {} }; struct W : virtual V {};
typedef int I;
void g();
int sameType() { try { g(); } catch (int) { return 1; }
    catch (int) { return 2; }                                       // C2312: S162
    return 0; }
int sameTypedef() { try { g(); } catch (int) { return 1; } catch (long) { return 2; }
    catch (I) { return 3; }                                         // C2312: S162, the first handler's line
    return 0; }
int laterReference() { try { g(); } catch (int) { return 1; }
    catch (const int&) { return 2; }                                // C2315: S162
    return 0; }
int earlierReference() { try { g(); } catch (const int&) { return 1; }
    catch (int) { return 2; }                                       // C2313: S162
    return 0; }
int qualified() { try { g(); } catch (const D&) { return 1; }
    catch (D&) { return 2; }                                        // C2312: S162
    return 0; }
int pointerQualified() { try { g(); } catch (const int*) { return 1; }
    catch (int*) { return 2; }                                      // C2312: S162
    return 0; }
int baseReference() { try { g(); } catch (B&) { return 1; }
    catch (D&) { return 2; }                                        // C4286: S162
    return 0; }
int basePointer() { try { g(); } catch (const B*) { return 1; }
    catch (E*) { return 2; }                                        // C4286: S162
    return 0; }
int baseValue() { try { g(); } catch (B) { return 1; }
    catch (D) { return 2; }                                         // C4286: S162
    return 0; }
int virtualBase() { try { g(); } catch (V&) { return 1; }
    catch (W&) { return 2; }                                        // C4286: S162
    return 0; }
template <class T> int inTemplate() { try { g(); } catch (int) { return 1; }
    catch (T) { return 2; }                                         // C2312 for T = int: S162, once
    return 0; }
int instantiated() { return inTemplate<int>() + inTemplate<long>(); }
// an array or a function in a handler is the pointer to it (15.3/2; row G29): cl C2312, clang warns
int adjustedArray() { try { g(); } catch (int*) { return 1; }
    catch (int [3]) { return 2; }                                   // C2312: S162
    return 0; }
int adjustedFunction() { try { g(); } catch (void ()) { return 1; }
    catch (void (*)()) { return 2; }                                // C2312: S162
    return 0; }
int adjustedNamed() { try { g(); } catch (int f(int)) { return f(1); } catch (const char s[]) { return s[0]; } return 0; }   // no warning
// no warning: the later handler can be entered
int fineOrder() { try { g(); } catch (D&) { return 1; } catch (B&) { return 2; } catch (...) { return 3; } return 0; }
int fineOther() { try { g(); } catch (int) { return 1; } catch (long) { return 2; } catch (int*) { return 3; } catch (const int*) { return 4; } return 0; }
int finePrivate() { try { g(); } catch (B&) { return 1; } catch (P&) { return 2; } return 0; }       // cl warns (C4286); B& does not catch a P (15.3/3)
int fineAmbiguous() { try { g(); } catch (B*) { return 1; } catch (M*) { return 2; } return 0; }     // cl warns; B* does not catch an M*
int fineConst() { try { g(); } catch (B*) { return 1; } catch (const D*) { return 2; } return 0; }   // cl and clang warn; B* does not catch a const D*
int fineNested() { try { try { g(); } catch (int) { return 1; } } catch (int) { return 2; } return 0; }   // two try blocks
// no warning, as cl: never entered, but cl says nothing
int quietReference() { try { g(); } catch (D&) { return 1; } catch (const D&) { return 2; } return 0; }
int quietValue() { try { g(); } catch (B) { return 1; } catch (D&) { return 2; } return 0; }
int quietVoid() { try { g(); } catch (void*) { return 1; } catch (int*) { return 2; } return 0; }
int main() { return 0; }
