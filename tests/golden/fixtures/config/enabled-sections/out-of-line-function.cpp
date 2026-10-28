/// \cond INTERNAL
namespace detail {

/// Hidden function.
void helper();

} // detail
/// \endcond

/// Out-of-line definition of the hidden function.
void detail::helper()
{
}

/// Visible function.
int visible(int x);
