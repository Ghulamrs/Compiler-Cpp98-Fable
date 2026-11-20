// A program for cppparse to read.
//
// "Run project" builds cppparse and then runs it, and cppparse is itself a
// reader of C++ sources: handed nothing it can only print its usage, and
// handed a folder it reports on every file under it. So the project names
// this one file in its .pro, and Run prints cppparse's report of it: the
// namespace, the classes with their members and bases, the functions, the
// enumeration - the same lines on every run.
//
// It is written in C++98, which is what cppparse reads.

namespace shapes {

enum Kind { Plain, Square, Round };

class Shape {
public:
    Shape() : sides_(0) {}
    virtual ~Shape() {}
    virtual int area() const { return 0; }
    virtual Kind kind() const { return Plain; }
    int sides() const { return sides_; }

protected:
    explicit Shape(int sides) : sides_(sides) {}

private:
    int sides_;
};

class Rect : public Shape {
public:
    Rect(int w, int h) : Shape(4), w_(w), h_(h) {}
    virtual int area() const { return w_ * h_; }
    virtual Kind kind() const { return w_ == h_ ? Square : Plain; }

private:
    int w_;
    int h_;
};

class Circle : public Shape {
public:
    explicit Circle(int r) : r_(r) {}
    virtual int area() const { return 3 * r_ * r_; }
    virtual Kind kind() const { return Round; }

private:
    int r_;
};

template <class T>
T larger(T a, T b)
{
    return a < b ? b : a;
}

int total(const Shape *const *all, int count)
{
    int sum = 0;
    for (int i = 0; i < count; ++i) sum += all[i]->area();
    return sum;
}

}  // namespace shapes

int main()
{
    shapes::Rect a(3, 4);
    shapes::Rect b(5, 5);
    shapes::Circle c(2);
    const shapes::Shape *all[3] = { &a, &b, &c };
    int sum = shapes::total(all, 3);
    return shapes::larger(sum, 40) == 49 ? 0 : 1;
}
