/// \cond INTERNAL
namespace detail {

/// Function of the hidden namespace.
int helper(int x);

/// Record of the hidden namespace.
struct Impl
{
    /// Member of the hidden record.
    int member;

    /// Function of the hidden record.
    void run();
};

} // detail
/// \endcond

/// Visible function.
int visible(int x);
