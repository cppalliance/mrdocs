/// \cond INTERNAL
/// Hidden record.
struct Impl
{
    /// Hidden function.
    void run();
};
/// \endcond

/// Out-of-line definition of a member of the hidden record.
void Impl::run()
{
}

/// Visible function.
int visible(int x);
