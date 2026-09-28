// Given: `derived : detail::crtp_base<derived>`, where the friend
//   `operator==(crtp_base, crtp_base)` uses the injected class name, so in
//   the instantiation it takes `crtp_base<derived>`.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined, so neither the primary template nor the
//   instantiation has a page.
// Expect: `lib::operator==(derived, derived)` listed under `derived`: an
//   operand naming a specialization whose primary has no page is rewritten to
//   the derived class.

namespace lib {

namespace detail {

/// Implementation-defined CRTP base parameterized on the derived type.
template <class Derived>
struct crtp_base
{
    /// Compares two derived values.
    friend bool operator==(crtp_base, crtp_base) { return true; }
};

}

/// Concrete CRTP derived class.
struct derived : detail::crtp_base<derived>
{
};

}  // namespace lib
