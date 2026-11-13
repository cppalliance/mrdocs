/// A physical quantity.
struct quantity {};

/// Whether `T` is a physical quantity.
template <class T>
concept Quantity = true;

/// A buffer of characters.
class buffer
{
public:
    /// Return the character at `i`.
    char& at(int i);

    /// Return the character at `i`.
    char const& at(int i) const;
};

/// A pair of values.
template <class T, class U>
struct pair {};

/// A pair of integers.
template <>
struct pair<int, int> {};
