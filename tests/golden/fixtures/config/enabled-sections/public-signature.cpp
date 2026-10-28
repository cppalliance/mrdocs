/// \cond INTERNAL
/// Hidden type.
struct Impl
{
    /// Hidden member.
    int member;
};
/// \endcond

/// Visible function that returns a hidden type.
Impl make();
