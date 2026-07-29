namespace A
{
enum class E {
    /// The constant to be introduced
    e1
};
}

using enum A::E;

void
f()
{
    e1; // This should resolve to A::E::e1
}
