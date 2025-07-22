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

#include <compare>
#include <exception>
#include <functional>
#include <typeinfo>
#include <utility>
#include <mrdocs/ADT/detail/Polymorphic.hpp>
#include <mrdocs/Support/Assert.hpp>
#include <type_traits>

namespace clang::mrdocs {

/** Default deleter for Polymorphic.

    This class template is used as the
    default deleter for the class
    template Polymorphic.
*/
template <class Base>
struct DefaultDeleter
{
    constexpr
    void
    operator()(Base const* p) const noexcept
    {
        delete p;
    }
};

/** Default copier for Polymorphic.

    This class template is used as the
    default copier for the class
    template Polymorphic.
*/
template <class Base>
struct DefaultCopier
{
    Base*
    operator()(Base const& t) const
    {
        return new Base(t);
    }
};

/** Exception thrown when a polymorphic value
    cannot be constructed.

    This exception is thrown when a polymorphic
    value cannot be constructed. This can happen
    when the type `T` is abstract or when the
    type `T` is not publicly derived from the
    type of the object being constructed.
*/
class BadPolymorphicConstruction
    : public std::exception
{
public:
    BadPolymorphicConstruction() noexcept = default;
    char const* what() const noexcept override {
        return "bad polymorphic value construction";
    }
};

template <class T>
class Polymorphic;

template <class T>
struct IsPolymorphic : std::false_type {};

template <class T>
struct IsPolymorphic<Polymorphic<T>> : std::true_type {};

template <class T>
inline constexpr bool IsPolymorphic_v = IsPolymorphic<T>::value;

/** A polymorphic value-type.

    This class supports polymorphic objects
    with value-like semantics.

    It works like std::optional in the sense
    that it can be engaged or disengaged. It can
    also be copied and moved.

    It also works like std::unique_ptr in the
    sense that it can be used to manage the
    lifetime of a polymorphic object. A
    Polymorphic can be hold an object
    of any class publicly derived from T
    and copying the Polymorphic will
    copy the object of the derived class.

    This combination also allows for a
    class that uses Polymorphic as
    a member to be copyable, movable,
    default constructible, default destructible,
    and comparable.

    These properties make ownership of members
    of composite objects clear and explicit.
    It also allows for the use of polymorphism
    with value-like semantics and default copy,
    move, assignment, destruction, and comparisons
    all available by default, without replicating
    boilerplate code in all derived classes.

    @par Deep copies

    To copy polymorphic objects, the class uses the
    copy constructor of the owned derived-type
    object when copying to another value.
    Similarly, to allow correct destruction of
    derived objects, it uses the destructor of
    the owned derived-type object in the
    destructor.

    @par Constructors

    A value can be constructed from a derived-type,
    a pointer to a derived type, and a pointer
    to a derived type with a custom copier and deleter
    function. When not supplied, a custom copier and
    deleter are provided. The value can also be
    constructed from a `std::unique_ptr`. A value
    cannot be constructed from an abstract class
    even if `T` is abstract.

    @par Default copier and deleter

    The standard library already provides a standard
    deleter for `std::unique_ptr`. The default copier
    is a pointer to a function using the copy
    constructor of the derived-type object.

    @par Empty State

    A value can be in an empty state. The state is
    useful for cheap default construction and
    for later assignment.

    This implementation is inspired by and
    adapted from p0201r5 and p3019r11.
*/
template<class Base>
class Polymorphic
{
    //
    // Assertions
    //
    static_assert(!std::is_union_v<Base>);
    static_assert(std::is_class_v<Base>);

    //
    // Members
    //

    // Block containing all the information about the object, copier
    // and deleter in a single allocation
    detail::PolymorphicStorage<Base>* s_{nullptr};

    // Local pointer to the object for convenience / performance
    Base* ptr_{nullptr};

    //
    // Friends
    //
    template <class U>
    friend class Polymorphic;

    template <
        class BaseU,
        std::derived_from<BaseU> Derived,
        class... Args>
    requires std::constructible_from<Derived, Args...>
    friend
    constexpr
    Polymorphic<BaseU>
    MakePolymorphic(Args&&... args);

    template <class Derived, class BaseU>
    requires std::derived_from<Derived, BaseU>
    friend
    constexpr
    Polymorphic<Derived>
    DynamicCast(Polymorphic<BaseU>&& other);

public:
    using element_type = Base;

    /** Default constructor.

        Ensures: *this is empty.
     */
    constexpr
    Polymorphic() noexcept = default;

    /** Constructs an empty Polymorphic.

        Ensures: *this is empty.
     */
    constexpr
    Polymorphic(std::nullptr_t) noexcept {}

    /** Constructs a Polymorphic which owns an object of type V.

        Constructs a Polymorphic which owns an object of type V,
        direct-non-list-initialized with std::forward<U>(u), where
        V is remove_cvref_t<U>.

        Throws: Any exception thrown by the selected constructor of V
        or bad_alloc if required storage cannot be obtained.

        Constraints: V* is convertible to T*.

        @tparam Derived The type of the object to be owned.
        @param other The object to be owned.

        @throws std::bad_alloc if required storage cannot be obtained.
     */
    template <std::derived_from<Base> Derived>
    requires (!IsPolymorphic_v<Derived>)
    explicit constexpr
    Polymorphic(Derived&& other)
        : Polymorphic(
            std::in_place_type<std::decay_t<Derived>>,
            std::forward<Derived>(other)) {}

    /** Constructs a Polymorphic which owns the object pointed to by p.

        If p is null, creates an empty object.
        If p is non-null creates an object that owns the object *p, with
        a copier and deleter initialized from std::move(c) and std::move(d).

        If p is non-null then the expression c(*p) has type U*.
        The expression d(p) is well-formed, has well-defined behavior,
        and does not throw exceptions.

        Move-initialization of objects of type C and D does not
        exit via an exception.

        A copier and deleter are said to be present in a non-empty
        object initialized with this constructor.

        Constraints: U* is convertible to T*.

        Expects: C and D meet the C++17 CopyConstructible and C++17
        Destructible requirements.

        @tparam Derived The type of the object to be owned.
        @tparam Copier The type of the copier.
        @tparam Deleter The type of the deleter.
        @param p Pointer to the object to be owned.
        @param c The copier.
        @param d The deleter.

        @throws std::bad_alloc if required storage cannot be obtained.
        @throws BadPolymorphicConstruction if
        std::same_as<C, DefaultCopier<U>>, std::same_as<D, DefaultDeleter<U>>
        and typeid(*p)!=typeid(U) are all true.
     */
    template <
        std::derived_from<Base> Derived,
        class Copier = DefaultCopier<Derived>,
        class Deleter = DefaultDeleter<Derived>>
    requires
        std::is_default_constructible_v<Copier> &&
        std::is_default_constructible_v<Deleter> &&
        (!std::is_pointer_v<Deleter>) &&
        std::copy_constructible<Copier> &&
        std::destructible<Copier> &&
        std::is_nothrow_move_constructible_v<Copier> &&
        std::is_nothrow_move_constructible_v<Deleter>
    explicit constexpr
    Polymorphic(Derived* p, Copier c, Deleter d)
    {
        // If p is null, creates an empty object.
        if (!p)
        {
            return;
        }

        // If using the default copier and deleter, check if the type of the object is correct
        if constexpr (std::same_as<Copier, DefaultCopier<Derived>> && std::same_as<Deleter, DefaultDeleter<Derived>>)
        {
            if (typeid(*p) != typeid(Derived))
            {
                throw BadPolymorphicConstruction();
            }
        }

        // Create a control block that stores the pointer, deleter, and copier
        using CBT = detail::PointerPolymorphicStorage<Base, Derived, Copier, Deleter>;
        s_ = new CBT(std::move(p), std::move(c), std::move(d));
        ptr_ = s_->ptr();
    }

    /// @copydoc Polymorphic(Derived*, Copier, Deleter)
    template <
        std::derived_from<Base> Derived,
        class Copier = DefaultCopier<Derived>>
    requires
        std::is_default_constructible_v<Copier> &&
        std::copy_constructible<Copier> &&
        std::destructible<Copier> &&
        std::is_nothrow_move_constructible_v<Copier>
    explicit constexpr
    Polymorphic(Derived* p, Copier c)
        : Polymorphic(p, c, DefaultDeleter<Derived>{}) {}

    /// @copydoc Polymorphic(Derived*, Copier, Deleter)
    template <std::derived_from<Base> Derived>
    explicit constexpr
    Polymorphic(Derived* p)
        : Polymorphic(p, DefaultCopier<Derived>{}, DefaultDeleter<Derived>{}) {}

    /** Copy constructor.

        If pv is empty, constructs an empty object. Otherwise, creates an object
        that owns a copy of the object managed by pv.

        If a copier and deleter are present in pv then the copy
        is created by the copier in pv.

        Otherwise, the copy is created by copy construction of the owned
        object.

        If a copier and deleter are present in pv then the copier and
        deleter of the object constructed are copied from those in pv.

        Throws: Any exception thrown by invocation of the copier,
        copying the copier and deleter, or bad_alloc if required
        storage cannot be obtained.

        @post bool(*this) == bool(pv).

        @param other The Polymorphic to copy from.

        @throws std::bad_alloc if required storage cannot be obtained.
     */
    constexpr
    Polymorphic(Polymorphic const& other)
    {
        if (!other)
        {
            return;
        }
        auto* tmp = other.s_->clone();
        ptr_ = tmp->ptr();
        s_ = std::move(tmp);
    }

    /** Copy constructor.

        If pv is empty, constructs an empty object. Otherwise, creates an object
        that owns a copy of the object managed by pv.

        If a copier and deleter are present in pv then the copy
        is created by the copier in pv.

        Otherwise, the copy is created by copy construction of the owned
        object.

        If a copier and deleter are present in pv then the copier and
        deleter of the object constructed are copied from those in pv.

        Throws: Any exception thrown by invocation of the copier,
        copying the copier and deleter, or bad_alloc if required
        storage cannot be obtained.

        Constraints: U* is convertible to T*.

        @post bool(*this) == bool(pv).

        @param other The Polymorphic to copy from.

        @throws std::bad_alloc if required storage cannot be obtained.
     */
    template <std::derived_from<Base> Derived>
    requires (!std::same_as<Derived, Base>)
    explicit constexpr
    Polymorphic(Polymorphic<Derived> const& other)
    {
        if (!other)
        {
            return;
        }
        auto tmp = other.s_->clone();
        using CBT = detail::DelegatingPolymorphicStorage<Base, Derived>;
        s_ = new CBT(tmp);
        ptr_ = s_->ptr();
    }

    /** Move constructor.

        If pv is empty, constructs an empty object.
        Otherwise, the object owned by pv is transferred to the
        constructed object.

        If a copier and deleter are present in pv then the
        copier and deleter are transferred to the
        constructed object.

        @post *this owns the object previously owned by pv (if any). pv is empty.

        @param other The Polymorphic to move from.
     */
    constexpr
    Polymorphic(Polymorphic&& other) noexcept
    {
        ptr_ = std::exchange(other.ptr_, nullptr);
        s_ = std::exchange(other.s_, nullptr);
    }

    /** Move constructor.

        If pv is empty, constructs an empty object.
        Otherwise, the object owned by pv is transferred to the
        constructed object.

        If a copier and deleter are present in pv then the
        copier and deleter are transferred to the
        constructed object.

        Constraints: U* is convertible to T*

        @post *this owns the object previously owned by pv (if any). pv is empty.

        @param other The Polymorphic to move from.
     */
    template <std::derived_from<Base> Derived>
    requires (!std::same_as<Derived, Base>)
    explicit constexpr
    Polymorphic(Polymorphic<Derived>&& other) noexcept
    {
        if (!other)
        {
            return;
        }
        using CBT = detail::DelegatingPolymorphicStorage<Base, Derived>;
        s_ = new CBT(std::exchange(other.s_, nullptr));
        ptr_ = s_->ptr();
        other.ptr_ = nullptr;
    }

    /** In-place constructor.
     */
    template <class Derived, class... Args>
    requires
        std::derived_from<std::decay_t<Derived>, Base> &&
        (!IsPolymorphic_v<std::decay_t<Derived>>) &&
        std::constructible_from<Derived, Args...>
    explicit constexpr
    Polymorphic(
        std::in_place_type_t<Derived>,
        Args&&... args)
            : s_(new detail::DirectPolymorphicStorage<Base, Derived>(std::forward<Args>(args)...))
            , ptr_(s_->ptr())
    {}

    /** Destructor.

        If a copier c and a deleter d are present, evaluates
        d(operator->()) and destroys c and d.

        Otherwise, destroys the owned object (if any).
     */
    constexpr
    ~Polymorphic()
    {
        delete s_;
        ptr_ = nullptr;
        s_ = nullptr;
    }

    /** Copy assignment operator.

        Equivalent to `Polymorphic(pv).swap(*this)`.

        No effects if an exception is thrown.

        Throws: Any exception thrown by the copier or bad_alloc if required storage cannot be obtained.

        @post The state of *this is as if copy constructed from pv.

        @param other The Polymorphic to copy from.
        @return Reference to *this.

        @throws std::bad_alloc if required storage cannot be obtained.
     */
    constexpr
    Polymorphic&
    operator=(Polymorphic const& other)
    {
        if (&other == this)
        {
            return *this;
        }

        if (!other)
        {
            delete s_;
            s_ = nullptr;
            ptr_ = nullptr;
            return *this;
        }

        auto* tmp = other.s_->clone();
        s_ = std::move(tmp);
        ptr_ = s_->ptr();
        return *this;
    }

    /** Move assignment operator.

        Equivalent to `Polymorphic(pv).swap(*this)`.

        @post The state of *this is as if copy constructed from pv.

        @param other The Polymorphic to copy from.
        @return Reference to *this.

        @throws std::bad_alloc if required storage cannot be obtained.
     */
    constexpr
    Polymorphic&
    operator=(Polymorphic&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        s_ = std::exchange(other.s_, nullptr);
        ptr_ = std::exchange(other.ptr_, nullptr);
        return *this;
    }

    /** Move assignment operator.

        Assign directly from a derived object.

        Equivalent to `Polymorphic(std::move(other)).swap(*this)`.

        This is an extension to the standard that
        extremely useful in MrDocs because it allows
        us to construct the object of the derived type
        on the stack and then assign it to a Polymorphic
        member of a class.

        @post The state of *this is as if copy constructed from other.

        @param other The derived object to copy from.
        @return Reference to *this.

        @throws std::bad_alloc if required storage cannot be obtained.
     */
    template <class Derived>
    requires std::derived_from<std::decay_t<Derived>, Base>
    constexpr
    Polymorphic&
    operator=(Derived&& other) noexcept
    {
        *this = Polymorphic(
            std::in_place_type<std::decay_t<Derived>>,
            std::forward<Derived>(other));
        return *this;
    }

    /** Exchanges the state of p and *this.

        @param other The Polymorphic to swap with.
     */
    constexpr
    void
    swap(Polymorphic& other) noexcept
    {
        using std::swap;
        swap(ptr_, other.ptr_);
        swap(s_, other.s_);
    }

    /** Returns a reference to the owned object.

        @return Reference to the owned object.

        @pre bool(*this) is true.
     */
    constexpr
    Base const&
    operator*() const noexcept(noexcept(*std::declval<Base*>()))
    {
        MRDOCS_ASSERT(ptr_);
        return *ptr_;
    }

    /** Returns a reference to the owned object.

        @return Reference to the owned object.

        @pre bool(*this) is true.
     */
    constexpr
    Base&
    operator*() noexcept(noexcept(*std::declval<Base*>()))
    {
        MRDOCS_ASSERT(ptr_);
        return *ptr_;
    }

    /** Returns a pointer to the owned object.

        @return Pointer to the owned object.

        @pre bool(*this) is true.
     */
    constexpr
    Base const*
    operator->() const noexcept
    {
        return ptr_;
    }

    /** Returns a pointer to the owned object.

        @return Pointer to the owned object.

        @pre bool(*this) is true.
     */
    constexpr
    Base*
    operator->() noexcept
    {
        return ptr_;
    }

    /** Checks if the Polymorphic owns an object.

        @return true if the Polymorphic owns an object, otherwise false.
     */
    constexpr
    explicit
    operator
    bool() const noexcept
    {
        return static_cast<bool>(ptr_);
    }
};

/** Creates a Polymorphic owning an object of type U.

    @tparam Base The type of the Polymorphic.
    @tparam Derived The type of the object to be owned, defaults to T.
    @tparam Args The types of the arguments to forward to the constructor of U.
    @param args The arguments to forward to the constructor of U.
    @return A Polymorphic<T> owning an object of type U direct-non-list-initialized with std::forward<Ts>(ts)...
 */
template <
    class Base,
    std::derived_from<Base> Derived = Base,
    class... Args>
requires std::constructible_from<Derived, Args...>
constexpr
Polymorphic<Base>
MakePolymorphic(Args&&... args)
{
    Polymorphic<Base> p;
    using BCT = detail::DirectPolymorphicStorage<Base, Derived>;
    p.s_ = new BCT(std::forward<Args>(args)...);
    p.ptr_ = p.s_->ptr();
    return p;
}

/** Dynamically cast the object owned by a Polymorphic.

    @tparam Derived The type to cast to.
    @tparam Base The type of the Polymorphic.
    @param other The Polymorphic to cast.
    @return A pointer to the object owned by other cast to Derived, or nullptr if the cast fails.
 */
template <class Derived, class Base>
requires std::derived_from<Derived, Base>
constexpr
Polymorphic<Derived>
DynamicCast(Polymorphic<Base>&& other)
{
    if (!other)
    {
        return {};
    }
    if (auto ptr = dynamic_cast<Derived*>(other.ptr_))
    {
        Polymorphic<Derived> result;
        using CBT = detail::DelegatingPolymorphicStorage<Derived, Base>;
        result.s_ = new CBT(std::move(other.s_));
        result.ptr_ = result.s_->ptr();
        other.s_ = nullptr;
        other.ptr_ = nullptr;
        return result;
    }
    other = {};
    return {};
}

/** Exchanges the state of two Polymorphic objects.

    @tparam Base The type of the Polymorphic.
    @param lhs The first Polymorphic to swap.
    @param rhs The second Polymorphic to swap.

    Effects: Equivalent to p.swap(u).
 */
template <class Base>
constexpr
void
swap(Polymorphic<Base>& lhs, Polymorphic<Base>& rhs) noexcept
{
    lhs.swap(rhs);
}

/** Determine if a Polymorphic is of a given type.

    @tparam Derived The type to check for.
    @tparam Base The type of the Polymorphic.
    @param pv The Polymorphic to check.
    @return true if pv owns an object of type Derived, otherwise false.
 */
template <class Derived, class Base>
requires std::derived_from<Derived, Base>
bool
IsA(Polymorphic<Base> const& pv)
{
    if (!pv)
    {
        return false;
    }
    return dynamic_cast<Derived const*>(std::addressof(*pv));
}

/** Retrieves the value of the Polymorphic<Base> as a qualified Derived type.

    This function can retrieve the value as a pointer, reference, or type name,
    and can handle const-qualified versions.

    If the requested type is not const, the input type should be mutable.
    If the requested type is const, the input type can be mutable or const.

    The function validates the derived type.
    If the object is not of the derived type, the pointer versions return
    `nullptr` and the reference/type name versions throw an exception.

    @tparam T The requested type (e.g., DerivedTypeName*, DerivedTypeName&, or DerivedTypeName).
    @tparam Base The base type of the Polymorphic.
    @param pv The Polymorphic to retrieve the value from.
    @return The value of the Polymorphic as the requested type.
    @throws std::bad_cast if the requested type is a reference or type name and the object is not of the derived type.
 */
template <class T, class Base>
requires
    std::is_pointer_v<T> &&
    std::derived_from<std::remove_pointer_t<T>, Base>
T
get(Polymorphic<Base>& pv)
{
    return dynamic_cast<T>(pv.operator->());
}

/// @copydoc get
template <class T, class Base>
requires
    std::is_reference_v<T> &&
    std::derived_from<std::remove_reference_t<T>, Base>
T
get(Polymorphic<Base>& pv)
{
    if (auto* ptr = dynamic_cast<std::remove_reference_t<T>*>(pv.operator->()))
    {
        return *ptr;
    }
    throw std::bad_cast();
}

/// @copydoc get
template <class T, class Base>
requires
    (!std::is_pointer_v<T>) &&
    (!std::is_reference_v<T>) &&
    std::derived_from<std::remove_pointer_t<T>, Base>
T&
get(Polymorphic<Base>& pv)
{
    return get<T&>(pv);
}

/// @copydoc get
template <class T, class Base>
requires
    std::is_pointer_v<T> &&
    std::derived_from<std::remove_pointer_t<T>, Base>
std::add_const_t<std::remove_pointer_t<T>>*
get(Polymorphic<Base> const& pv)
{
    return dynamic_cast<std::add_const_t<std::remove_pointer_t<T>>*>(pv.operator->());
}

/// @copydoc get
template <class T, class Base>
requires
    std::is_reference_v<T> &&
    std::derived_from<std::remove_reference_t<T>, Base>
std::add_const_t<std::remove_reference_t<T>>&
get(Polymorphic<Base> const& pv)
{
    if (auto* ptr = dynamic_cast<std::add_const_t<std::remove_reference_t<T>>*>(pv.operator->()))
    {
        return *ptr;
    }
    throw std::bad_cast();
}

/// @copydoc get
template <class T, class Base>
requires
    (!std::is_pointer_v<T>) &&
    (!std::is_reference_v<T>) &&
    std::derived_from<std::remove_pointer_t<T>, Base>
std::add_const_t<T>&
get(Polymorphic<Base> const& pv)
{
    return get<T const&>(pv);
}

/** Compares two Polymorphic that have visit functions

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

/// @copydoc CompareDerived
template <class Base>
requires
    (!IsPolymorphic_v<Base>) &&
    detail::CanVisitCompare<Base>
auto
CompareDerived(Base const& lhs, Base const& rhs)
{
    if (lhs.Kind == rhs.Kind)
    {
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
