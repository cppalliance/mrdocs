// Reproduces issue #1176: members defined in a class template base do not
// appear in derived classes that inherit from a specialization of that base.
//
// Boost.Multi's `array_ref` derives from `subarray`, and `subarray`'s
// `operator[]` did not show up on `array_ref`'s documentation page,
// even with `inherit-base-members: copy-all`.

/// A class template base.
template <typename T>
class base
{
public:
    /// Indexing operator that should be inherited.
    T& operator[](int n);

    /// A regular member function that should also be inherited.
    T& method();
};

/// A class template that inherits from `base<T>`.
template <typename T>
class derived
    : public base<T>
{};

/// A non-template that inherits from a concrete specialization.
class concrete_derived
    : public base<int>
{};
