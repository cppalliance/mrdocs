/// A class whose relational operators sit at the
/// bottom of the member list.
struct value
{
    /// Get the underlying integer.
    int get() const;
    /// Equality comparison.
    bool operator==(const value&) const;
    /// Less-than comparison.
    bool operator<(const value&) const;
};
