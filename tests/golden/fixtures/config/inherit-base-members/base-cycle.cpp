// A self-referential CRTP base: `Facade`'s own base is a specialization of
// `Facade`, which the primary-template-ID fallback for dependent bases resolves
// back to the primary template. That makes the base graph cycle back to the
// record being finalized. BaseMembersFinalizer must break the cycle with its
// `finalized_` guard instead of recursing until the stack overflows.

template <typename T> struct Facade;

/// A self-referential CRTP facade.
template <typename Derived>
struct Facade : Facade<Facade<Derived>>
{
    /// A member the facade provides.
    void method();
};
