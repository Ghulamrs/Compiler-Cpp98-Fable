// typeid-refused.cpp - must be rejected: typeid with a std::type_info that the program defines itself instead of
// including <typeinfo> (cl C2027: the class of typeid's result is cl's own type_info, undefined without the header)
namespace std { class type_info { public: const char* name() const { return "mine"; } int x; }; }
int main() { const std::type_info& t = typeid(int); return t.name()[0]; }
