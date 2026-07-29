namespace A
{
    enum class E {
        /// The constant to be introduced
        e1
    };
}

class B {
public:
    using enum A::E; // This should resolve to A::E::e1
};

void
g()
{
    B::e1;
}