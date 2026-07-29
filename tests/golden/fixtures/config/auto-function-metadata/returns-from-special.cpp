/// A helper class
struct A {};

bool
operator==(A const&, A const&);

bool
operator!=(A const&, A const&);

bool
operator<(A const&, A const&);

bool
operator<=(A const&, A const&);

bool
operator>(A const&, A const&);

bool
operator>=(A const&, A const&);

bool
operator!(A const&);

auto
operator<=>(A const&, A const&);

struct Undoc {};

/// Test class
struct X
{
    operator A() const;

    operator Undoc() const;

    X&
    operator=(A const&);

    X&&
    operator=(A&&);

    X
    operator+(X const&) const;

    X*
    operator->();

    bool
    operator==(X const&) const;

    bool
    operator!=(X const&) const;

    bool
    operator<(X const&) const;

    bool
    operator<=(X const&) const;

    bool
    operator>(X const&) const;

    bool
    operator>=(X const&) const;

    bool
    operator!() const;

    auto
    operator<=>(X const&) const;
};

/// A fake output stream
struct ostream;

ostream&
operator<<(ostream&, A const&);
