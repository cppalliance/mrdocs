template <class T>
struct A {
    /// Implicit conversion from double
    A(double);

    /// Explicit conversion from int
    explicit
    A(int);

    /// Conditionally explicit conversion from char
    explicit(sizeof(T) > 4)
    A(char);

    /// Conditionally explicit conversion from other types
    template <typename U>
    explicit(sizeof(U) > 4)
    A(U);

    /// Implicit conversion to double
    operator
    double() const;

    /// Explicit conversion to bool
    explicit
    operator bool() const;

    /// Conditionally explicit conversion to char
    explicit(sizeof(T) > 4)
    operator char() const;

    /// Conditional explicit conversion to other types
    template <typename U>
    explicit(sizeof(U) > 1)
    operator U() const;
};
