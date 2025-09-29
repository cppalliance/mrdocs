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

#include <mrdocs/ADT/Nullable.hpp>
#include <mrdocs/Support/Assert.hpp>
#include <concepts>
#include <utility>

namespace mrdocs {

/** A polymorphic value-type.

    This class supports polymorphic objects
    with value-like semantics.

    It implements a tweaked version of std::polymorphic, based on the
    reference implementation for P3019R14. Differences are:
    * It implements comparison operators with a very project-specific design.
    * Fixed allocator, not parametrizable.
    * No initializer_list constructor.

    @par Deep copies

    To copy polymorphic objects, the class uses the
    copy constructor of the owned derived-type
    object when copying to another value.
    Similarly, to allow the correct destruction of
    derived objects, it uses the destructor of
    the owned derived-type object in the
    destructor.
*/
template <class T>
class Polymorphic {
    // Base class for type-erasure.
    struct WrapperBase {
        virtual constexpr ~WrapperBase() = default;
        virtual T& getValue() = 0;
        virtual constexpr WrapperBase* clone() = 0;
    };

    template <class U>
    class Wrapper final : public WrapperBase {
        U Value;
        U& getValue() override { return Value; }
    public:
        template <class... Ts>
        constexpr Wrapper(Ts&&... ts) : Value(std::forward<Ts>(ts)...) {}
        constexpr ~Wrapper() override = default;
        constexpr WrapperBase* clone() override { return new Wrapper(Value); }
    };

    // nullptr only when constructed/reset by nullable_traits
    WrapperBase* WB = nullptr;

    // Private null token and constructor: only the friend traits can call this.
    struct _null_t { explicit constexpr _null_t(int) {} };
    explicit constexpr Polymorphic(_null_t) noexcept : WB(nullptr) {}

    // Allow the traits specialization to access private members/ctors.
    friend struct nullable_traits<Polymorphic<T>>;

    // std::polymorphic has a default constructor that
    // default-constructs the base type T if it's default-constructible.
    // We disable this constructor because this is always an error
    // in MrDocs.
    // constexpr explicit Polymorphic()
    // requires std::is_default_constructible_v<T>
    //          && std::is_copy_constructible_v<T>
    //     : WB(new Wrapper<T>())
    // {}

public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = T const*;

    /// Default constructs the value type (never null via public API).
    /// explicit constexpr Polymorphic() : Polymorphic(std::in_place_type<T>) {}

    /** Forwarding constructor from a derived U. */
    template <class U>
    constexpr explicit Polymorphic(U&& u)
        requires (!std::same_as<Polymorphic, std::remove_cvref_t<U>>) &&
                 std::copy_constructible<std::remove_cvref_t<U>> &&
                 std::derived_from<std::remove_cvref_t<U>, T>
        : WB(new Wrapper<std::remove_cvref_t<U>>(std::forward<U>(u))) {}

    /** In-place constructor for a specific derived U.

        @param ts Arguments to forward to U's constructor.
     */
    template <class U, class... Ts>
    explicit constexpr Polymorphic(std::in_place_type_t<U>, Ts&&... ts)
        requires std::same_as<std::remove_cvref_t<U>, U> &&
                 std::constructible_from<U, Ts&&...> &&
                 std::copy_constructible<U> && std::derived_from<U, T>
        : WB(new Wrapper<U>(std::forward<Ts>(ts)...)) {}

    constexpr Polymorphic(Polymorphic const& V)
        : WB(V.WB ? V.WB->clone() : nullptr) {}

    constexpr Polymorphic(Polymorphic&& V) noexcept
        : WB(std::exchange(V.WB, nullptr)) {}

    constexpr ~Polymorphic() { delete WB; }

    constexpr Polymorphic& operator=(Polymorphic const& V) {
        if (this != &V) {
            Polymorphic Temp(V);
            swap(*this, Temp);
        }
        return *this;
    }

    constexpr Polymorphic& operator=(Polymorphic&& V) noexcept {
        if (this != &V) {
            delete WB;
            WB = std::exchange(V.WB, nullptr);
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

    [[nodiscard]] constexpr T& operator*() noexcept {
        MRDOCS_ASSERT(WB != nullptr);
        return WB->getValue();
    }

    [[nodiscard]] constexpr T const& operator*() const noexcept {
        MRDOCS_ASSERT(WB != nullptr);
        return WB->getValue();
    }

    constexpr bool
    valueless_after_move() const noexcept
    {
        return WB == nullptr;
    }

    friend constexpr void
    swap(Polymorphic& lhs, Polymorphic& rhs) noexcept
    {
        std::swap(lhs.WB, rhs.WB);
    }
};

namespace detail {

// A function to compare two polymorphic objects that
// store the same derived type
template <class Base>
struct VisitCompareFn
{
    Base const& rhs;

    template <class Derived>
    auto
    operator()(Derived const& lhsDerived)
    {
        auto const& rhsDerived = dynamic_cast<Derived const&>(rhs);
        return lhsDerived <=> rhsDerived;
    }
};

template <class Base>
concept CanVisitCompare = requires(Base const& b)
{
    visit(b, VisitCompareFn<Base>{ b });
};

template <class T> inline constexpr bool IsPolymorphic = false;
template <class T> inline constexpr bool IsPolymorphic<Polymorphic<T>> = true;

} // namespace detail

/// @copydoc CompareDerived(Polymorphic<Base> const&, Polymorphic<Base> const&)
template <class Base>
requires(!detail::IsPolymorphic<Base>) && detail::CanVisitCompare<Base>
auto
CompareDerived(Base const& lhs, Base const& rhs)
{
    if (lhs.Kind == rhs.Kind)
    {
        return visit(lhs, detail::VisitCompareFn<Base>(rhs));
    }
    return lhs.Kind <=> rhs.Kind;
}

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
CompareDerived(Polymorphic<Base> const& lhs, Polymorphic<Base> const& rhs)
{
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    return CompareDerived(*lhs, *rhs);
}

template <class Base>
requires detail::CanVisitCompare<Base>
auto
operator<=>(Polymorphic<Base> const& lhs, Polymorphic<Base> const& rhs)
{
    return CompareDerived(lhs, rhs);
}

template <class Base>
requires detail::CanVisitCompare<Base>
bool
operator==(Polymorphic<Base> const& lhs, Polymorphic<Base> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}

// -------------------- nullable_traits specialization --------------------

/** nullable_traits for Polymorphic<T>.

    Only this friend specialization can create/reset the null state.
*/
template<class T>
struct nullable_traits<Polymorphic<T>>
{
    static constexpr bool
    is_null(Polymorphic<T> const& v) noexcept
    {
        return v.valueless_after_move();
    }

    static constexpr Polymorphic<T>
    null() noexcept
    {
        // Use the private null constructor
        return Polymorphic<T>(typename Polymorphic<T>::_null_t{0});
    }

    static constexpr void
    make_null(Polymorphic<T>& v) noexcept
    {
        delete v.WB;
        v.WB = nullptr;
    }
};

//------------------------------------------------------------------------------
// isa<T>(p)
//------------------------------------------------------------------------------

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline bool
isa(const Polymorphic<From>& p) noexcept
{
    if (p.valueless_after_move()) return false;
    using ToU = std::remove_reference_t<To>;
    return dynamic_cast<const ToU*>(std::addressof(*p)) != nullptr;
}

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline bool
isa_or_null(const Polymorphic<From>* pp) noexcept
{
    return pp && isa<To>(*pp);
}

//------------------------------------------------------------------------------
// dyn_cast<T>(p)  → pointer (nullptr if not)
//------------------------------------------------------------------------------

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline auto
dyn_cast(Polymorphic<From>& p) noexcept
    -> std::add_pointer_t<std::remove_reference_t<To>>
{
    if (p.valueless_after_move()) return nullptr;
    using ToU = std::remove_reference_t<To>;
    return dynamic_cast<ToU*>(std::addressof(*p));
}

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline auto
dyn_cast(const Polymorphic<From>& p) noexcept
    -> std::add_pointer_t<const std::remove_reference_t<To>>
{
    if (p.valueless_after_move()) return nullptr;
    using ToU = std::remove_reference_t<To>;
    return dynamic_cast<const ToU*>(std::addressof(*p));
}

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline auto
dyn_cast_or_null(Polymorphic<From>* pp) noexcept
    -> std::add_pointer_t<std::remove_reference_t<To>>
{
    return (pp && !pp->valueless_after_move()) ? dyn_cast<To>(*pp) : nullptr;
}

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline auto
dyn_cast_or_null(const Polymorphic<From>* pp) noexcept
    -> std::add_pointer_t<const std::remove_reference_t<To>>
{
    return (pp && !pp->valueless_after_move()) ? dyn_cast<To>(*pp) : nullptr;
}

//------------------------------------------------------------------------------
// cast<T>(p)  → reference (asserts on failure)
//------------------------------------------------------------------------------

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline auto
cast(Polymorphic<From>& p)
    -> std::remove_reference_t<To>&
{
    assert(!p.valueless_after_move() && "cast: moved-from Polymorphic");
    auto* r = dyn_cast<To>(p);
    assert(r && "cast<T>: invalid cast");
    return *r;
}

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline auto
cast(const Polymorphic<From>& p)
    -> const std::remove_reference_t<To>&
{
    assert(!p.valueless_after_move() && "cast: moved-from Polymorphic");
    auto* r = dyn_cast<To>(p);
    assert(r && "cast<T>: invalid cast");
    return *r;
}

//------------------------------------------------------------------------------
// cast_or_null<T>(pp)  → pointer (asserts on bad non-null)
//------------------------------------------------------------------------------

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline auto
cast_or_null(Polymorphic<From>* pp)
    -> std::add_pointer_t<std::remove_reference_t<To>>
{
    if (!pp) return nullptr;
    auto* r = dyn_cast<To>(*pp);
    assert(r && "cast_or_null<T>: invalid cast on non-null argument");
    return r;
}

template <class To, class From>
requires ( std::derived_from<std::remove_cvref_t<To>, std::remove_cvref_t<From>> )
[[nodiscard]] inline auto
cast_or_null(const Polymorphic<From>* pp)
    -> std::add_pointer_t<const std::remove_reference_t<To>>
{
    if (!pp) return nullptr;
    auto* r = dyn_cast<To>(*pp);
    assert(r && "cast_or_null<T>: invalid cast on non-null argument");
    return r;
}

} // namespace mrdocs

#endif // MRDOCS_API_ADT_POLYMORPHIC_HPP
