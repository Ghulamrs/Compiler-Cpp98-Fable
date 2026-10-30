// qualconv-refused.cpp - must be rejected: qualification conversions 4.4/4 does not allow, a const added below a
// level without const (cl C2440, "conversion loses qualifiers"); front.cpp qualconv:: has the valid ones
int main()
{
    int x = 5; int* px = &x; int** ppx = &px;
    const int** bad = ppx;
    int* const** bad2 = &ppx;
    const int* volatile* const* bad3 = &ppx;
    return **bad + ***bad2 + ***bad3;
}
