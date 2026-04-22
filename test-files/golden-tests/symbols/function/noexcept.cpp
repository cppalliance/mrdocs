// A function with an unconditional noexcept-specifier.
void f1() noexcept;

// A function with noexcept(true).
void f2() noexcept(true);

// A function with noexcept(false).
void f3() noexcept(false);

// Traits declared locally, so the fixture does not depend on
// <type_traits>.
template <typename T>
inline constexpr bool is_nothrow_swappable_v = false;
template <typename T>
inline constexpr bool is_nothrow_move_constructible_v = false;
template <typename T>
inline constexpr bool is_nothrow_move_assignable_v = false;

// A function template with a short conditional noexcept whose
// operand depends on a type trait. The operand is short enough
// to be rendered inline in the signature.
template <typename T>
void f4(T&) noexcept(is_nothrow_swappable_v<T>);

// A function template with a simpler conditional noexcept: a
// dependent expression that is not a type trait. Also short
// enough to be inlined.
template <typename T>
void f5(T a, T b) noexcept(noexcept(a + b));

// A function template whose noexcept operand is long enough to
// be rendered as `noexcept(see-below)` in the signature, with
// the actual condition shown in a dedicated "noexcept
// Specification" section. See issue #1103.
template <typename T>
void f6(T&, T&) noexcept(
    is_nothrow_move_constructible_v<T> &&
    is_nothrow_move_assignable_v<T>);
