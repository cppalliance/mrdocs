// Test that a conversion function is named with the type as written.

/// An alias for a builtin.
using MyAlias = short;

/// A trait stand-in for a constrained conversion.
template <typename T>
struct IsArray
{
    /// Whether the type is an array.
    static constexpr bool value = false;
};

/// A class template with conversion functions.
template <typename T>
struct Holder
{
    /// Convert to the class template parameter.
    explicit operator T();

    /// Convert to a reference to the class template parameter.
    explicit operator T&();

    /// Convert to a dependent type.
    explicit operator typename T::type();

    /// Convert to a reference to a function template parameter.
    template <class CArray>
    explicit operator CArray&();

    /// Convert to a function template parameter.
    template <class Range>
    explicit operator Range() const;

    /// Convert through an alias.
    explicit operator MyAlias();

    /// Convert through a constrained template with a defaulted parameter.
    template <
        class Range,
        class = decltype(Range(0))>
    requires (!IsArray<Range>::value)
    constexpr
    explicit
    operator Range() const;
};
