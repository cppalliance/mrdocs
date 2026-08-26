/// A documented class with an undocumented hidden friend.
///
/// A hidden friend is never reached by a Regular traversal, so the extraction
/// time set of undocumented symbols missed it. The finalized-corpus scan is
/// what surfaces it (the warning goes to stderr, which the golden harness does
/// not capture yet).
struct Widget
{
    friend bool operator==(Widget const&, Widget const&) { return true; }
};

/// A documented class template.
template <class T>
struct Box
{
    /// A documented member.
    void get();
};

/// A documented explicit specialization. Its member is folded onto the primary,
/// so an undocumented member here must not be reported as undocumented.
template <>
struct Box<int>
{
    void get();
};
