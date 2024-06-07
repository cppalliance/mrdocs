struct S
{
    static const S s;
};
constexpr S S::s = S{};

struct T
{
    static constinit const int t = 0;
};

constexpr int T::t;
