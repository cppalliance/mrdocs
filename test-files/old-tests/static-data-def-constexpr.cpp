struct S
{
    static const S s;
};
constexpr S S::s = S{};
