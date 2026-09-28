// The hidden-friend note renders on a friend declared only inside its class,
// naming that class, and is absent on a friend that has a matching
// namespace-scope declaration. A friend copied onto a derived class under
// `inherit-hidden-friends` names the derived class and the base that declares
// the friend (`wrapped : base`), except that an implementation-defined base is
// never named (`value : detail::ops`, with `detail` implementation-defined).

/// A class whose `operator==` is a hidden friend: declared (and defined) only
/// inside the class body, so it is found only through argument-dependent
/// lookup.
struct point
{
    /// Compare two points.
    friend bool
    operator==(point a, point b)
    {
        return a.x == b.x;
    }

    double x;
};

/// A class whose friend also has a matching namespace-scope declaration, so it
/// is an ordinary (non-hidden) friend and renders no admonition.
struct label
{
    /// Compare two labels.
    friend bool
    operator==(label a, label b);
};

/// The matching namespace-scope declaration; `label`'s friend is not hidden.
bool
operator==(label a, label b)
{
    return true;
}

/// A documented base carrying a hidden friend.
struct base
{
    /// Order two bases.
    friend bool
    less(base a, base b)
    {
        return true;
    }
};

/// Derives from the documented base; the copied friend names `base`.
struct wrapped : base
{
};

namespace detail {

/// An implementation-defined base carrying a hidden friend.
struct ops
{
    /// Order two ops.
    friend bool
    less(ops a, ops b)
    {
        return true;
    }
};

}

/// Derives from the implementation-defined base; the copied friend cannot
/// name it.
struct value : detail::ops
{
};
