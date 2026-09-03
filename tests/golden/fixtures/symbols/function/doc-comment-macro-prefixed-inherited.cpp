// A documented member whose declaration starts with a macro (an attribute
// macro) must keep its doc when the member is copied into a derived class
// from an instantiated base. The comment lookup for the copy is anchored at
// the declaration's begin location, which for these members is a macro
// location, so it must be mapped back to the file first.

#define NODISCARD [[ nodiscard ]]

/// A class template.
template <class T>
struct S {
    /// Documented in-class, defined out of class, begins with a macro.
    NODISCARD int h();

    /// Documented in-class, defined out of class, no macro.
    int k();

    /// Documented in-class, defined in class, begins with a macro.
    NODISCARD int m() { return 0; }
};

template <class T>
int S<T>::h() { return 0; }

template <class T>
int S<T>::k() { return 0; }

/// Derives from an instantiation and inherits its documented members.
struct D : S<char> {};
