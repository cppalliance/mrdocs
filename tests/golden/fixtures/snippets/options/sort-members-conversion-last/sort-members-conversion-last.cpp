/// A class whose conversion operators are pushed to
/// the bottom of the member list.
struct value
{
    /// Read the underlying integer.
    int get() const;
    /// Convert to `bool` for use in conditions.
    explicit operator bool() const;
    /// Convert to `int` for arithmetic contexts.
    explicit operator int() const;
};
