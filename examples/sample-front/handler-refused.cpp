// handler-refused.cpp - must be rejected by --compile6 (S161): a handler after catch (...) (C++98 15.3/6: "..."
// shall be the last handler of its try block; cl C2311, clang "catch-all handler must come last"). Row G23
void g();
int notLast() { try { g(); } catch (...) { return 1; }
    catch (int) { return 2; }
    return 0; }
int twice() { try { g(); } catch (int) { return 1; } catch (...) { return 2; }
    catch (...) { return 3; }
    return 0; }
int functionTry() try { g(); return 0; } catch (...) { return 1; }
    catch (long) { return 2; }
// S163: a handler for an incomplete type, or for a pointer or a reference to one (15.3/1; row G29). cl refuses the
// type itself (C2687) and the reference (C2316) and takes the pointer; clang refuses the three. void*, a pointer to
// a pointer, a reference to a pointer and a pointer to member are complete types and are taken
struct Inc; union IncU; typedef Inc* IncPtr; template <class T> struct Only;
int incPointer() { try { g(); }
    catch (Inc*) { return 1; }
    catch (const Inc* const) { return 2; }
    catch (IncPtr) { return 3; }
    return 0; }
int incReference() { try { g(); }
    catch (Inc&) { return 1; }
    catch (const IncU& u) { return &u != 0; }
    return 0; }
int incValue() { try { g(); }
    catch (Inc) { return 1; }
    return 0; }
int incVoid() { try { g(); }
    catch (void) { return 1; }
    return 0; }
int incArray() { try { g(); }
    catch (int (*)[]) { return 1; }
    catch (int (&)[]) { return 2; }
    return 0; }
int incSpecialization() { try { g(); }
    catch (Only<int>*) { return 1; }
    return 0; }
template <class T> int incDependent() { try { g(); }
    catch (T*) { return 1; }
    return 0; }
int incInstantiated() { return incDependent<int>() + incDependent<Inc>(); }
struct Whole { int x; };
int incFine() { try { g(); } catch (void*) { return 1; } catch (const volatile void*) { return 2; } catch (Inc**) { return 3; } catch (Inc*&) { return 4; }
    catch (int Inc::*) { return 5; } catch (Whole*) { return 6; } catch (Whole&) { return 7; } catch (int (*)[3]) { return 8; } return 0; }
int main() { return notLast() + twice() + functionTry() + incPointer() + incReference() + incValue() + incVoid() + incArray() + incSpecialization() + incInstantiated() + incFine(); }
