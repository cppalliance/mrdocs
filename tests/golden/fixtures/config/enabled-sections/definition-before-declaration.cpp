/// \cond INTERNAL
/// Hidden definition.
struct Widget
{
    /// Hidden member.
    int member;
};
/// \endcond

/// Visible redeclaration.
struct Widget;

/// Visible function that returns the type.
Widget make();
