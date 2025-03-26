/// A helper tag
struct A
{};

/// Test class
struct X
{
    X(X const& other);

    X(X&& other);

    X(A const& value);

    X(A&& value);

    X(int value);

    X&
    operator=(X const& other);

    X&
    operator=(X&& other);

    X&
    operator=(A const& value);

    X&
    operator=(A&& value);

    X&
    operator=(int value);
};
