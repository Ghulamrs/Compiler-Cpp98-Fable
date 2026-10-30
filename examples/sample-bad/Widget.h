// Widget.h - deliberately broken header used to test the health checker.
// (no include guard on purpose: W102)
#include "Gadget.h"
#include "nothere.h"                 // E100 include not found

class Gadget;                        // forward declaration only, never defined

class Base {
public:
    Base();
    virtual void run();
    virtual int  value() const = 0;
    ~Base();                         // W125 non-virtual destructor in a polymorphic base
};

class Widget : public Base, public Base {   // E121 duplicate base
public:
    Widget(int id, const char* label);
    void   run();
    void   run();                    // E123 duplicate declaration
    int    value() const;
    double scale(double factor = 1.0);
    void   helper();                 // W132 declared, never defined
    int    count;
    int    count;                    // E123 duplicate field
    Gadget gadget;                   // E124 field of incomplete type
    static int s_instances;          // W126 never defined
    int    m_id;
    const char* m_label;
}                                    // E110 missing ';'

class Tool : public Base {
public:
    Tool();
    void run(int level);             // W128 hides virtual Base::run(); I127 Tool stays abstract (value() not implemented)
};

class Ext : public Unknown {         // W122 base class not defined
public:
    int x;
};

struct Dup { int a; };               // E120 also defined in Other.cpp

int  g_header_global = 5;            // W164 global defined in a header
void headerFunction() { }            // W165 non-inline function defined in a header
extern int g_never_defined;          // W161 never defined
int  declaredOnly(int x);            // W133 declared, never defined
