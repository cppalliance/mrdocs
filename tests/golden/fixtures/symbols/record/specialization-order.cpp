// Test that specializations and deduction guides are listed in a
// predictable order. Both lists take their name from the primary, so
// what decides is what a specialization was specialized with, and what
// a guide deduces with the types it takes breaking a tie. Types rank by
// kind, a named type before a reference before a pointer. Everything
// below is declared out of that order on purpose.

/// Primary class template.
template <typename T, typename U>
struct A
{
};

/// Specialization for a pointer.
template <typename T, typename U>
struct A<T*, U>
{
};

/// Specialization for two ints.
template <>
struct A<int, int>
{
};

/// Specialization for a reference.
template <typename T, typename U>
struct A<T&, U>
{
};

/// Class template with deduction guides.
template <typename T>
struct B
{
    /// Constructor.
    B(T);
};

/// Guide taking a pointer.
template <typename T>
B(T*) -> B<T>;

/// Guide taking a char.
B(char) -> B<int>;

/// Guide taking a bool.
B(bool) -> B<int>;

/// Guide deducing a different specialization, so what it deduces decides
/// its place rather than what it takes.
B(int) -> B<char>;
