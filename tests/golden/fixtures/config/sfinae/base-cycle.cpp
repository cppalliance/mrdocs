// Regression test for issue #1270: SFINAE detection walks the base classes
// of every specialization of a template looking for the member that carries
// the SFINAE result. When two templates derive from each other through
// their specializations, that walk must stop instead of recursing forever.
// This mirrors boost::gil::iterator_is_step / detail::iterator_is_step_impl.

template <class I>
struct is_step;

namespace detail {

template <class It, bool IsBase>
struct is_step_impl;

/// Base iterators are never step iterators
template <class It>
struct is_step_impl<It, true>
{
    static constexpr bool value = false;
};

/// Adaptors are step iterators when their base is
template <class It>
struct is_step_impl<It, false>
    : is_step<typename It::base_t>
{
};

} // namespace detail

/// Whether the iterator has a dynamic step
template <class I>
struct is_step
    : detail::is_step_impl<I, I::is_base>
{
};

/// Uses the cyclic template as a plain type
template <class I>
struct step_holder
{
    detail::is_step_impl<I, false> impl;
};

/// Uses the cyclic template through a dependent member
template <class I>
typename is_step<I>::type
step_type(I it);
