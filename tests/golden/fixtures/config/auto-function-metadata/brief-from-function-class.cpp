/// A helper tag
struct A
{};

/// Test class
struct X
{
    X() = default;
    X(X const&) = default;
    X(X&&) = default;
    X(A const&);
    X(A&&);
    X(int);
    ~X();
    operator int() const;
    operator A() const;
};