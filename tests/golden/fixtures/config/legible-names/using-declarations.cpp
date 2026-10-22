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
