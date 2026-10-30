// main.cpp - entry point of the sample project.
// Exercises: include of a file not listed in the project (Legacy.h), a
// missing include (Missing.h), file-static and const globals with array
// initializers, typedef of a function pointer, anonymous namespace,
// local variables of template type, try/catch in main.
#include "Shape.h"
#include "Container.h"
#include "Legacy.h"      // not listed in the project: found through #include
#include "Missing.h"     // does not exist: reported in the health section
#include <cstdio>

using namespace geom;

static int s_runs = 0;
const int kTable[] = { 1, 2, 3 };
typedef void (*ReportFn)(const Shape&);

namespace {
    void report(const Shape& s) { std::printf("%s\n", s.kind()); }
}

int main(int argc, char* argv[])
{
    int exitCode = 0;
    ReportFn fn = &report;
    Canvas canvas;
    util::FixedStack<int> ints;
    LegacyCounter counter;
    (void)argc;
    (void)argv;
    try {
        canvas.add(new Circle(1.5));
        canvas.add(new Rect(2.0, 3.0));
        for (int i = 0; i < 3; ++i) ints.push(kTable[i]);
        double total = canvas.totalArea();
        std::printf("total area %f\n", total);
        counter.tick();
        ++s_runs;
    }
    catch (const std::exception& e) {
        std::printf("error: %s\n", e.what());
        exitCode = 1;
    }
    return exitCode;
}
