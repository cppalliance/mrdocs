/// A helper tag
struct A
{};

/// A dumb ostream class
struct ostream {};

/// Test class
struct X
{
    X&
    operator=(X const&);

    X&
    operator=(X&&);

    X&
    operator=(A const&);

    X&
    operator+=(X const&);
};

ostream&
operator<<(ostream& os, X const& x)
{
    return os;
}
