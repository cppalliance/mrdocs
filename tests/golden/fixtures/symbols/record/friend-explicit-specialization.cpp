// A friend that names an explicit specialization must resolve to that
// specialization, not its primary template. Regression test for
// `canonicalFriendTarget`, which used to collapse every friend to its
// primary template.

/// A function template.
template <class T>
void
f(T)
{
}

/// The tag type the specialization is written for.
struct X {};

/// An explicit specialization of `f` for `X`.
template <>
void
f<X>(X);

/// A class that befriends the explicit specialization.
struct Y
{
    friend void f<X>(X);
};
