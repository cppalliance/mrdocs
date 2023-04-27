//Test conversion operators that can be "explicit", "explicit(true)", "explicit(false)" and "explicit(EXPR)"

struct Explicit {
    explicit operator bool();
};

struct ExplicitFalse {
    explicit(false) operator bool();
};

struct ExplicitTrue {
    explicit(true) operator bool();
};

template<bool B>
struct ExplicitExpression {
    explicit(B) operator bool();
};