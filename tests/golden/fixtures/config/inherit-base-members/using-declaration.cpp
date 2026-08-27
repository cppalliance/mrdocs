// A member re-exported from a private base with a using-declaration is a
// member of the class that re-exports it, so a class deriving from that
// one inherits it like any other member.
//
// `Shadowing` declares the same name for itself, which hides the name
// the base re-exported, so only its own member is listed.

struct Base
{
    int size() const;
};

struct Mid : private Base
{
    using Base::size;

    int count() const;
};

struct Leaf : Mid {};

struct Deeper : Leaf {};

struct Shadowing : Mid
{
    void size(int);
};
