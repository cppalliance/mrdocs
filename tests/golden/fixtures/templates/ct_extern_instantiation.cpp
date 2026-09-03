// `extern template` and `template` instantiations are uses of a template,
// not declarations or definitions of anything: A<int>, A<long>, and g<int>
// introduce nothing new and carry nothing to document, so only the primary
// templates appear in the output.

/// A class template.
template <typename T>
struct A {
    /// A member.
    void f();
};

/// A function template.
template <typename T>
void g(T value);

extern template struct A<int>;
template struct A<long>;
extern template void g<int>(int);
