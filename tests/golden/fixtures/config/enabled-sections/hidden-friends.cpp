namespace detail {

struct access;

void poke();

} // detail

/// Brief of Widget.
struct Widget
{
    /// \cond INTERNAL
    friend struct detail::access;
    friend void detail::poke();
    /// \endcond

    friend struct Visible;
};
