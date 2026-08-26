// A using-declaration re-exporting a member of a base that is filtered
// out of the documentation. There is nothing left to point at, so the
// declaration records only the name it introduces, the same as one off
// a base that is a template parameter.

namespace ns {

/// A base marked as an implementation detail.
struct Detail
{
    /// A member of the detail base.
    int hidden() const;
};

/// A base excluded from extraction.
struct Excluded
{
    /// A member of the excluded base.
    int gone() const;
};

/// Re-export a member of each.
struct Reexport : private Detail, private Excluded
{
    /// Re-exported from the implementation-defined base.
    using Detail::hidden;

    /// Re-exported from the excluded base.
    using Excluded::gone;
};

} // namespace ns
