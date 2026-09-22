// A using-declaration off a base that depends on a template parameter
// names nothing until the template is instantiated, so the anchor cannot
// be taken from what it introduces, and the name it is written with may
// contain characters that are invalid for a file name or URL. The same
// declarations off a concrete base are here for comparison, since those
// name something already and take its anchor.

/// A base template with operators to re-export.
template <class T>
struct BaseTemplate
{
    /// Indexed access.
    T& operator[](int i);

    /// Call.
    void operator()();

    /// Convert to bool.
    explicit operator bool() const;
};

/// Re-export the operators of a base that depends on a template parameter.
template <class T>
struct Dependent : BaseTemplate<T>
{
    /// Indexed access, re-exported.
    using BaseTemplate<T>::operator[];

    /// Call, re-exported.
    using BaseTemplate<T>::operator();

    /// Conversion, re-exported.
    using BaseTemplate<T>::operator bool;
};

/// A base with operators to re-export.
struct Base
{
    /// Indexed access.
    int& operator[](int i);
};

/// Re-export the operators of a concrete base.
struct Concrete : Base
{
    /// Indexed access, re-exported.
    using Base::operator[];
};
