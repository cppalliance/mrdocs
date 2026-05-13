/// Primary class template.
template <typename T>
struct A
{
    /// A member function.
    void f();
};

/// Explicit specialization of the primary.
template <>
struct A<int>
{
};

/// Class template with a deduction guide.
template <int>
struct B
{
};

/// Deduction guide for B.
B(bool) -> B<0>;
