//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_ADT_POLYMORPHIC_HPP
#define MRDOCS_API_ADT_POLYMORPHIC_HPP

#include <concepts>
#include <mrdocs/Support/Assert.hpp>
#include <optional>
#include <utility>

namespace clang::mrdocs {

/** A polymorphic value-type.

    This class supports polymorphic objects
    with value-like semantics.

    It implements a tweaked version of std::polymorphic, based on the
    reference implementation for P3019R14. Differences are:
    * It supports nullability, which was originally supported in P3019 through
      std::optional specializations, but was later removed from the paper.
    * It implements comparison operators with a very project specific design.
    * Fixed allocator, not parametrizable.
    * No initializer_list constructor.

    @par Deep copies

    To copy polymorphic objects, the class uses the
    copy constructor of the owned derived-type
    object when copying to another value.
    Similarly, to allow correct destruction of
    derived objects, it uses the destructor of
    the owned derived-type object in the
    destructor.
*/
template <class T> class Polymorphic {
  struct WrapperBase {
    virtual constexpr ~WrapperBase() = default;
    virtual T &getValue() = 0;
    virtual constexpr WrapperBase *clone() = 0;
  };

  template <class U> class Wrapper final : public WrapperBase {
    U Value;

    U &getValue() override { return Value; }

  public:
    template <class... Ts>
    constexpr Wrapper(Ts &&...ts) : Value(std::forward<Ts>(ts)...) {}
    constexpr ~Wrapper() override = default;
    constexpr WrapperBase *clone() override { return new Wrapper(getValue()); }
  };

  WrapperBase *WB;

public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = T const *;

  /** Empty constructor.

      Ensures: *this is empty.
   */
  constexpr Polymorphic(std::nullopt_t) : WB(nullptr) {}

  /// Default constructs the value type.
  explicit constexpr Polymorphic() : Polymorphic(std::in_place_type<T>) {}

  /** Forwarding constructor.
   * @param u The object to copy.
   */
  template <class U>
  constexpr explicit Polymorphic(U &&u)
    requires(not std::same_as<Polymorphic, std::remove_cvref_t<U>>) &&
            std::copy_constructible<std::remove_cvref_t<U>> &&
            std::derived_from<std::remove_cvref_t<U>, T>
      : WB(new Wrapper<std::remove_cvref_t<U>>(std::forward<U>(u))) {}

  /** In place constructor
   * @param ts Arguments to forward to the constructor of U.
   */
  template <class U, class... Ts>
  explicit constexpr Polymorphic(std::in_place_type_t<U>, Ts &&...ts)
    requires std::same_as<std::remove_cvref_t<U>, U> &&
             std::constructible_from<U, Ts &&...> &&
             std::copy_constructible<U> && std::derived_from<U, T>
      : WB(new Wrapper<U>(std::forward<Ts>(ts)...)) {}

  constexpr Polymorphic(const Polymorphic &V)
      : WB(V ? V.WB->clone() : nullptr) {}

  constexpr Polymorphic(Polymorphic &&V) noexcept
      : WB(std::exchange(V.WB, nullptr)) {}

  constexpr ~Polymorphic() { delete WB; }

  constexpr Polymorphic &operator=(const Polymorphic &V) {
    if (this != &V) {
      Polymorphic Temp(V);
      swap(*this, Temp);
    }
    return *this;
  }

  constexpr Polymorphic &operator=(Polymorphic &&V) noexcept {
    if (this != &V) {
      swap(*this, V);
      Polymorphic Temp = std::nullopt;
      swap(V, Temp);
    }
    return *this;
  }

  [[nodiscard]] constexpr pointer operator->() noexcept {
    MRDOCS_ASSERT(WB != nullptr);
    return &WB->getValue();
  }

  [[nodiscard]] constexpr const_pointer operator->() const noexcept {
    MRDOCS_ASSERT(WB != nullptr);
    return &WB->getValue();
  }

  [[nodiscard]] constexpr T &operator*() noexcept {
    MRDOCS_ASSERT(WB != nullptr);
    return WB->getValue();
  }

  [[nodiscard]] constexpr const T &operator*() const noexcept {
    MRDOCS_ASSERT(WB != nullptr);
    return WB->getValue();
  }

  [[nodiscard]] explicit constexpr operator bool() const noexcept {
    return WB != nullptr;
  }

  friend constexpr void swap(Polymorphic &lhs, Polymorphic &rhs) noexcept {
    std::swap(lhs.WB, rhs.WB);
  }
};

namespace detail {

// A function to compare two polymorphic objects that
// store the same derived type
template <class Base> struct VisitCompareFn {
  Base const &rhs;

  template <class Derived> auto operator()(Derived const &lhsDerived) {
    auto const &rhsDerived = dynamic_cast<Derived const &>(rhs);
    return lhsDerived <=> rhsDerived;
  }
};

/** Determines if a type can be visited with VisitCompareFn
 */
template <class Base>
concept CanVisitCompare =
    requires(Base const &b) { visit(b, VisitCompareFn<Base>{b}); };

template <class T> inline constexpr bool IsPolymorphic = false;
template <class T> inline constexpr bool IsPolymorphic<Polymorphic<T>> = true;

} // namespace detail

/** @brief Compares two polymorphic objects that have visit functions

    This function compares two Polymorphic objects that
    have visit functions for the Base type.

    The visit function is used to compare the two objects
    if they are of the same derived type.

    If the two objects are of different derived types, the
    comparison is based on the type_info of the derived types.

    If any of the objects is empty, the comparison is based
    on the emptiness of the objects.

    @tparam Base The type of the Polymorphic.
    @param lhs The first Polymorphic to compare.
    @param rhs The second Polymorphic to compare.
    @return true if the two Polymorphic objects are equal, otherwise false.
 */
template <class Base>
requires detail::CanVisitCompare<Base>
auto
CompareDerived(
    Polymorphic<Base> const& lhs,
    Polymorphic<Base> const& rhs)
{
    if (lhs && rhs)
    {
        if (lhs->Kind == rhs->Kind)
        {
            return visit(*lhs, detail::VisitCompareFn<Base>(*rhs));
        }
        return lhs->Kind <=> rhs->Kind;
    }
    return !lhs ? std::strong_ordering::less
            : std::strong_ordering::greater;
}

/// @copydoc CompareDerived(Polymorphic<Base> const&, Polymorphic<Base> const&)
template <class Base>
  requires(!detail::IsPolymorphic<Base>) && detail::CanVisitCompare<Base>
auto CompareDerived(Base const &lhs, Base const &rhs) {
  if (lhs.Kind == rhs.Kind) {
    return visit(lhs, detail::VisitCompareFn<Base>(rhs));
  }
  return lhs.Kind <=> rhs.Kind;
}

template <class Base>
requires detail::CanVisitCompare<Base>
auto
operator<=>(
    Polymorphic<Base> const& lhs,
    Polymorphic<Base> const& rhs)
{
    return CompareDerived(lhs, rhs);
}

template <class Base>
requires detail::CanVisitCompare<Base>
bool
operator==(
    Polymorphic<Base> const& lhs,
    Polymorphic<Base> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}

} // clang::mrdocs

#endif
