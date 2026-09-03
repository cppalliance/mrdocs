// One macro invocation expands to two functions on one line, a static
// overload taking a predicate and a member overload taking nothing, and both
// receive the comment written above the macro. A documented parameter that
// either of them has is valid for both, so `\param P` below is not reported
// as nonexistent on the member overload.

#define WRAP_BOTH(Name)                                                 \
    static bool Name(int P) { return P > 0; }                           \
    bool Name() const { return Name(v); }

/// A comparison.
struct Cmp {
    /// The value.
    int v;

    /// Return true if the predicate is positive.
    /// \param P Predicate to test.
    /// \return Whether it is positive.
    WRAP_BOTH(isPositive);
};
