/// Visible forward declaration.
struct Widget;

/// \cond INTERNAL
/// Hidden definition.
struct Widget
{
    /// Hidden member.
    int member;
};
/// \endcond

/// Visible function.
int hidden_body(int x);
