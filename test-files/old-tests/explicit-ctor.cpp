//Test constructors that can be "explicit", "explicit(true)", "explicit(false)" and "explicit(EXPR)"

struct Explicit {
    explicit Explicit();
    explicit Explicit(const Explicit&);
    explicit Explicit(Explicit&&) noexcept;
    explicit Explicit(int, int);
};

struct ExplicitTrue {
    explicit(true) ExplicitTrue();
    explicit(true) ExplicitTrue(const ExplicitTrue&);
    explicit(true) ExplicitTrue(ExplicitTrue&&) noexcept;
    explicit(true) ExplicitTrue(int, int);
};

struct ExplicitFalse {
    explicit(false) ExplicitFalse();
    explicit(false) ExplicitFalse(const ExplicitFalse&);
    explicit(false) ExplicitFalse(ExplicitFalse&&) noexcept;
    explicit(false) ExplicitFalse(int, int);
};

template<bool B>
struct ExplicitExpression {
    explicit(B) ExplicitExpression();
    explicit(B) ExplicitExpression(const ExplicitExpression&);
    explicit(B) ExplicitExpression(ExplicitExpression&&) noexcept;
    explicit(B) ExplicitExpression(int, int);
};