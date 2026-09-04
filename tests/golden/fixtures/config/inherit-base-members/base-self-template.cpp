// A partial specialization inheriting its own primary template with different
// arguments (`Foo<T*> : Foo<T>`) is well-formed and must still inherit: the
// partial specialization gets its own SymbolID, and its base `Foo<T>` resolves
// to the primary template's (different) ID, so it is not the self-reference the
// cycle guard breaks. `base_m` must appear on `Foo<T*>`.

/// Primary template.
template <class T>
struct Foo
{
    /// A base member.
    void base_m();
};

/// Partial specialization for pointers, inheriting the primary.
template <class T>
struct Foo<T*> : Foo<T>
{
    /// A pointer-only member.
    void ptr_m();
};
