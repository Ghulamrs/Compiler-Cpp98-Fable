// Shape.h - class hierarchy for the sample project.
// Exercises: namespaces, abstract base class, multiple inheritance (one
// private, one virtual), nested struct, static members, bit fields,
// friend operator, typedef and enum inside a class, conversion operator,
// function pointer member, conditional declarations, dead code in #if 0,
// extern "C" and extern globals.
#ifndef SAMPLE_SHAPE_H
#define SAMPLE_SHAPE_H

#include <string>
#include <vector>
#include <iosfwd>
#include "Config.h"

namespace geom {

class SAMPLE_API Named {
public:
    explicit Named(const std::string& name);
    virtual ~Named();
    const std::string& name() const { return m_name; }
    void rename(const std::string& name);
protected:
    std::string m_name;
};

class SAMPLE_API Shape {
public:
    Shape();
    virtual ~Shape();
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual const char* kind() const;
    static int liveCount();
    Shape& operator=(const Shape& other);
    operator bool() const;
    friend std::ostream& operator<<(std::ostream& os, const Shape& s);

    typedef std::vector<Shape*> List;
    enum Flags { FlagNone = 0, FlagHidden = 1 << 0, FlagLocked = 1 << 1 };

protected:
    unsigned    m_flags : 4;
    mutable int m_cachedArea;

private:
    static int s_liveCount;
    Shape(const Shape&);            // not copyable
};

class Circle : public Shape, private Named {
public:
    Circle(double radius, const std::string& name = "circle");
    virtual ~Circle();
    virtual double area() const;
    virtual double perimeter() const;
    virtual const char* kind() const { return "circle"; }
    double radius() const { return m_radius; }
private:
    double m_radius;
};

class Rect : public Shape, public virtual Named {
public:
    Rect(double w, double h);
    virtual double area() const;
    virtual double perimeter() const;

    struct Corner {
        double x, y;
        Corner() : x(0), y(0) {}
    };
    Corner corner(int index) const;

private:
    double m_w, m_h;
    Corner m_corners[4];
};

#ifdef SAMPLE_WITH_3D
class Sphere : public Circle {
public:
    Sphere(double r);
    double volume() const;
};
#endif

#if 0
class Obsolete : public Shape {     // dead code: must not appear in the report
    void broken(;
};
#endif

class Canvas {
public:
    Canvas();
    ~Canvas();
    void   add(Shape* s);
    double totalArea() const;
    size_t count() const { return m_shapes.size(); }
private:
    Shape::List m_shapes;
    Rect        m_bounds;
    void      (*m_callback)(const Shape&);
};

SAMPLE_API double totalArea(const Shape::List& shapes);
std::ostream& operator<<(std::ostream& os, const Shape& s);

} // namespace geom

extern int g_shapeVersion;
extern "C" int sample_c_api(int value);

#endif // SAMPLE_SHAPE_H
