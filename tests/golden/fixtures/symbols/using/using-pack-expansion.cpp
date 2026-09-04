// A using-declaration can name a member of every base in a pack. The
// qualifier depends on the pack, so nothing is named until the template
// is instantiated and the declaration has no target to record.

/// A base with a member to re-export.
struct First
{
    /// Return the size.
    int size() const;
};

/// Another base with a member of the same name.
struct Second
{
    /// Return the size.
    int size() const;
};

/// Re-export the member from every base in the pack.
template <class... Bases>
struct FromPack : private Bases...
{
    /// Re-exported from each base in the pack.
    using Bases::size...;
};
