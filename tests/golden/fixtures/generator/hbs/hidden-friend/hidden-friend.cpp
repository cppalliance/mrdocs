// The hidden-friend note renders on a friend declared only inside its class,
// naming that class, and is absent on a friend that has a matching
// namespace-scope declaration.

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
