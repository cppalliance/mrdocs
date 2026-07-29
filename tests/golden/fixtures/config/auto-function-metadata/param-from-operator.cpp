/// A helper tag
struct A
{};

/// A dumb ostream class
struct ostream {};

/// Test class
struct X
{
    X
    operator+(X const& x) const;
};

X
operator-(X const& x, X const& y);

X
operator!(X const& x);

ostream&
operator<<(ostream& os, X const& x)
{
    return os;
}
