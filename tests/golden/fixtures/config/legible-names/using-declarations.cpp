// Using-declarations of one name add `using` and the class they name it
// in, or `using` alone when they name it in no class.
struct First
{
    void get(int n);
};

struct Second
{
    void get(double d);
};

struct Both : First, Second
{
    using First::get;
    using Second::get;
};

void f(int n);

namespace M
{
void f(double d);
}

namespace N
{
using ::f;
using M::f;
}

// Qualifiers that end in the same name are spelled in full, so the two
// using-declarations below add `A-B` and `C-B`.
namespace A::B
{
void g(int n);
}

namespace C::B
{
void g(double d);
}

namespace P
{
using A::B::g;
using C::B::g;
}

// A qualifier too long to spell is replaced by a hash of its spelling.
namespace a_namespace_whose_name_is_much_too_long_to_spell_in_an_anchor::inner
{
void h(int n);
}

namespace Q
{
void h(double d);
}

namespace R
{
using a_namespace_whose_name_is_much_too_long_to_spell_in_an_anchor::inner::h;
using Q::h;
}
