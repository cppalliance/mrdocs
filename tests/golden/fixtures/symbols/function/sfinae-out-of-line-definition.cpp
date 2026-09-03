// A SFINAE parameter must disappear from the signature on every
// redeclaration, not only on the first one seen. The out-of-line definition
// repopulates the parameters, and the requires clause is written once.

namespace bsl {
/// Primary: no `type` member.
template <bool C, class T = void>
struct enable_if {};

/// Enabled: exposes `type`.
template <class T>
struct enable_if<true, T> {
    /// The enabled type.
    typedef T type;
};

/// Integral trait.
template <class T>
struct is_integral {
    /// Whether T is integral.
    static const bool value = false;
};

/// Integral trait for int.
template <>
struct is_integral<int> {
    /// Whether int is integral.
    static const bool value = true;
};
} // namespace bsl

/// A tag type used as the SFINAE result.
struct Nil {};

/// A string reference.
template <class CHAR>
struct StringRefImp {
    /// Assign from a pointer and an integral length.
    ///
    /// @param data the data
    /// @param length the length
    template <class INT_TYPE>
    void assign(const CHAR *data, INT_TYPE length,
                typename bsl::enable_if<bsl::is_integral<INT_TYPE>::value,
                                        Nil>::type nil = Nil());
};

template <class CHAR>
template <class INT_TYPE>
void StringRefImp<CHAR>::assign(
    const CHAR *data, INT_TYPE length,
    typename bsl::enable_if<bsl::is_integral<INT_TYPE>::value, Nil>::type)
{
}

/// The char instantiation.
typedef StringRefImp<char> StringRef;
