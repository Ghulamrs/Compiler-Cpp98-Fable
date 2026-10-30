// Shape.cpp - implementation of the sample class hierarchy.
// Exercises: anonymous namespace, static member definition, constructor
// initializer lists, function-try-block, try/catch with several handlers,
// throw, local variables (including inside for loops), out-of-line
// definitions of nested types, extern "C" definition.
#include "Shape.h"
#include "Container.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    const double kPi = 3.14159265358979;
    int s_instances = 0;
    void bump(int delta) { s_instances += delta; }
}

int g_shapeVersion = 3;

void sample::trace(const char* msg)
{
    std::cerr << "[trace] " << msg << std::endl;
}

namespace geom {

int Shape::s_liveCount = 0;

Named::Named(const std::string& name) : m_name(name) {}
Named::~Named() {}
void Named::rename(const std::string& name) { m_name = name; }

Shape::Shape() : m_flags(FlagNone), m_cachedArea(-1)
{
    ++s_liveCount;
    bump(1);
}

Shape::~Shape()
{
    --s_liveCount;
    bump(-1);
}

const char* Shape::kind() const { return "shape"; }
int Shape::liveCount() { return s_liveCount; }

Shape& Shape::operator=(const Shape& other)
{
    m_flags = other.m_flags;
    return *this;
}

Shape::operator bool() const { return (m_flags & FlagHidden) == 0; }

std::ostream& operator<<(std::ostream& os, const Shape& s)
{
    return os << s.kind() << " area=" << s.area();
}

Circle::Circle(double radius, const std::string& name)
try : Shape(), Named(name), m_radius(radius)
{
    if (radius < 0.0) throw std::invalid_argument("negative radius");
}
catch (const std::invalid_argument& e)
{
    SAMPLE_TRACE(e.what());
    throw;
}

Circle::~Circle() {}

double Circle::area() const
{
    double r2 = m_radius * m_radius;
    return kPi * r2;
}

double Circle::perimeter() const { return 2.0 * kPi * m_radius; }

Rect::Rect(double w, double h) : Named("rect"), m_w(w), m_h(h)
{
    for (int i = 0; i < 4; ++i) {
        Corner c;
        c.x = (i == 1 || i == 2) ? w : 0.0;
        c.y = (i >= 2) ? h : 0.0;
        m_corners[i] = c;
    }
}

double Rect::area() const { return m_w * m_h; }
double Rect::perimeter() const { return 2.0 * (m_w + m_h); }

Rect::Corner Rect::corner(int index) const
{
    if (index < 0 || index > 3) throw std::out_of_range("corner index");
    return m_corners[index];
}

Canvas::Canvas() : m_bounds(0.0, 0.0), m_callback(0) {}

Canvas::~Canvas()
{
    for (Shape::List::iterator it = m_shapes.begin(); it != m_shapes.end(); ++it) delete *it;
}

void Canvas::add(Shape* s)
{
    if (!s) return;
    m_shapes.push_back(s);
    if (m_callback) m_callback(*s);
}

double Canvas::totalArea() const
{
    double sum = 0.0;
    size_t n = m_shapes.size();
    try {
        for (size_t i = 0; i < n; ++i) {
            const Shape* s = m_shapes[i];
            sum += s->area();
        }
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        sum = -1.0;
    }
    catch (...) {
        sum = -2.0;
    }
    return sum;
}

double totalArea(const Shape::List& shapes)
{
    util::FixedStack<double, 32> stack;
    double sum = 0.0, extra = 1.0;
    for (Shape::List::const_iterator it = shapes.begin(); it != shapes.end(); ++it)
        stack.push((*it)->area());
    while (!stack.empty()) sum += stack.pop();
    return sum * extra;
}

} // namespace geom

extern "C" int sample_c_api(int value)
{
    return value * 2;
}
