//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_LAZYOBJECT_HPP
#define MRDOCS_API_DOM_LAZYOBJECT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error.hpp>
#include <string_view>

namespace clang::mrdocs::dom {

namespace detail
{
    /* A class representing an archetypal IO object.
     */
    struct MRDOCS_DECL ArchetypalIO
    {
        template <class T>
        void
        map(std::string_view, T const&) const {}

        template <class F>
        void
        defer(std::string_view, F const&) const {}
    };

    /* A helper empty struct
     */
    struct NoLazyObjectContext { };
}

/** Customization point tag.

    This tag type is used by the class
    @ref dom::LazyObjectImpl to select overloads
    of `tag_invoke`.

    @note This type is empty; it has no members.

    @see @ref dom::LazyObjectImpl
    <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf">
        tag_invoke: A general pattern for supporting customisable functions</a>
*/
struct LazyObjectMapTag { };

/** Concept to determine if a type can be mapped to a
    @ref dom::LazyObjectImpl with a user-provided conversion.

    This concept determines if the user-provided conversion is
    defined as:

    @code
    template <class IO>
    void tag_invoke( LazyObjectMapTag, IO&, T& );
    @endcode

    This customization can be defined by any type that needs to be converted
    to/from a lazy @ref dom::Object. For example:

    @code
    template <class IO>
    void tag_invoke( LazyObjectMapTag, IO& io, MyStruct const& s)
    {
        io.map("name", s.name);
        io.map("size", s.size);
        io.map("age",  s.age);
    }
    @endcode

 */
template<class T>
concept HasLazyObjectMapWithoutContext = requires(
    detail::ArchetypalIO& io,
    T const& t)
{
    { tag_invoke(LazyObjectMapTag{}, io, t) } -> std::same_as<void>;
};

/** Concept to determine if a type can be mapped to a
    @ref dom::LazyObjectImpl with a user-provided conversion.

    This concept determines if the user-provided conversion is
    defined as:

    @code
    template <class IO>
    void tag_invoke( LazyObjectMapTag, IO&, T,  Context const& );
    @endcode
 */
template<class T, class Context>
concept HasLazyObjectMapWithContext = requires(
    detail::ArchetypalIO& io,
    T const& t,
    Context const& ctx)
{
    { tag_invoke(LazyObjectMapTag{}, io, t, ctx) } -> std::same_as<void>;
};

/** Determine if `T` can be converted to @ref dom::Value.

    If `T` can be converted to @ref dom::Value via a
    call to @ref dom::ValueFrom, the static data member `value`
    is defined as `true`. Otherwise, `value` is
    defined as `false`.
*/
template <class T, class Context>
concept HasLazyObjectMap =
    HasLazyObjectMapWithContext<T, Context> ||
    HasLazyObjectMapWithoutContext<T>;

//------------------------------------------------
//
// LazyObjectImpl
//
//------------------------------------------------

/** Lazy object implementation.

    This interface is used to define objects
    whose members are evaluated on demand
    as they are accessed.

    When any of the object properties are accessed,
    the object @ref dom::Value is constructed.
    In practice, the object never takes any memory
    besides the pointer to the underlying object.

    The keys and values in the underlying object
    should be mapped using `tag_invoke`.

    This class is typically useful for
    implementing objects that are expensive
    and have recursive dependencies, as these
    recursive dependencies can also be deferred.

    A context can also be stored in the object
    as a form to customize how the object is
    mapped. This context should be copyable
    and is propagated to other objects that
    support an overload with the same context.

    The context can be simply a tag
    identifying how to map the object, or
    a more complex object carrying data
    to customize the mapping process.

    In the latter case, because the context
    should be a copyable, the user might want
    to use a type with reference semantics.

*/
template <class T, class Context = detail::NoLazyObjectContext>
requires HasLazyObjectMap<T, Context>
class LazyObjectImpl : public ObjectImpl
{
    T const* underlying_;
    Object overlay_;
    MRDOCS_NO_UNIQUE_ADDRESS Context context_{};

public:
    explicit
    LazyObjectImpl(T const& obj)
        requires HasLazyObjectMapWithoutContext<T>
        : underlying_(&obj)
        , context_{} {}

    explicit
    LazyObjectImpl(T const& obj, Context const& context)
        requires HasLazyObjectMapWithContext<T, Context>
        : underlying_(&obj)
        , context_(context) {}

    ~LazyObjectImpl() override = default;

    /// @copydoc ObjectImpl::type_key
    char const*
    type_key() const noexcept override
    {
        return "LazyObject";
    }

    /// @copydoc ObjectImpl::get
    Value
    get(std::string_view key) const override;

    /// @copydoc ObjectImpl::set
    void
    set(String key, Value value) override;

    /// @copydoc ObjectImpl::visit
    bool
    visit(std::function<bool(String, Value)>) const override;

    /// @copydoc ObjectImpl::size
    std::size_t
    size() const override;

    /// @copydoc ObjectImpl::exists
    bool
    exists(std::string_view key) const override;
};

namespace detail
{
    /* The IO object for lazy objects.

       Mapping traits use this object to call the
       map and defer methods, which are used to
       access properties of the lazy object.

       Each function provides different behavior
       to `map` and `defer` methods, allowing
       to implement functionality such as
       `get`, `set`, `visit`, and `size`.

       In some cases, only a function for
       `map` is provided, and the `defer` function
       is not used. In this case, the `defer` function
       also uses the `map` function, which is
       the default behavior.

       In other cases, the `defer` function is
       used to defer the evaluation of a property
       to a later time, which is useful for functionality
       that requires accessing the value.
     */
    template <class MapFn, class DeferFn = void*>
    class LazyObjectIO
    {
        MapFn mapFn;
        DeferFn deferFn;
    public:
        explicit
        LazyObjectIO(MapFn mapFn, DeferFn deferFn = {})
            : mapFn(mapFn), deferFn(deferFn) {}

        template <class T>
        void
        map(std::string_view name, T const& value)
        {
            mapFn(name, value);
        }

        template <class F>
        void
        defer(std::string_view name, F&& deferred)
        {
            if constexpr (std::same_as<DeferFn, void*>)
            {
                mapFn(name, deferred);
            }
            else
            {
                deferFn(name, deferred);
            }
        }
    };

    // Deduction guide
    template <class MapFn, class DeferFn = void*>
    LazyObjectIO(MapFn, DeferFn = {}) -> LazyObjectIO<MapFn, DeferFn>;
}

template <class T, class Context>
requires HasLazyObjectMap<T, Context>
std::size_t
LazyObjectImpl<T, Context>::
size() const
{
    std::size_t result = 0;
    detail::LazyObjectIO io(
        [&result, this](std::string_view name, auto const& /* value or deferred */)
        {
            result += !overlay_.exists(name);
        });
    if constexpr (HasLazyObjectMapWithContext<T, Context>)
    {
        tag_invoke(LazyObjectMapTag{}, io, *underlying_, context_);
    }
    else
    {
        tag_invoke(LazyObjectMapTag{}, io, *underlying_);
    }
    return result + overlay_.size();
}

template <class T, class Context>
requires HasLazyObjectMap<T, Context>
bool
LazyObjectImpl<T, Context>::
exists(std::string_view key) const
{
    if (overlay_.exists(key))
    {
        return true;
    }
    bool result = false;
    detail::LazyObjectIO io(
        [&result, key](std::string_view name, auto const& /* value or deferred */)
    {
        if (!result && name == key)
        {
            result = true;
        }
    });
    if constexpr (HasLazyObjectMapWithContext<T, Context>)
    {
        tag_invoke(LazyObjectMapTag{}, io, *underlying_, context_);
    }
    else
    {
        tag_invoke(LazyObjectMapTag{}, io, *underlying_);
    }
    return result;
}


template <class T, class Context>
requires HasLazyObjectMap<T, Context>
Value
LazyObjectImpl<T, Context>::
get(std::string_view key) const
{
    if (overlay_.exists(key))
    {
        return overlay_.get(key);
    }
    Value result;
    detail::LazyObjectIO io(
        [&result, key, this](std::string_view name, auto const& value)
        {
            if (result.isUndefined() && name == key)
            {
                if constexpr (HasValueFromWithContext<std::remove_cvref_t<decltype(value)>, Context>)
                {
                    ValueFrom(value, context_, result);
                }
                else
                {
                    ValueFrom(value, result);
                }
            }
        }, [&result, key, this](std::string_view name, auto const& deferred)
        {
            if (result.isUndefined() && name == key)
            {
                if constexpr (HasValueFromWithContext<std::remove_cvref_t<decltype(deferred())>, Context>)
                {
                    ValueFrom(deferred(), context_, result);
                }
                else
                {
                    ValueFrom(deferred(), result);
                }
            }
        });
    if constexpr (HasLazyObjectMapWithContext<T, Context>)
    {
        tag_invoke(LazyObjectMapTag{}, io, *underlying_, context_);
    }
    else
    {
        tag_invoke(LazyObjectMapTag{}, io, *underlying_);
    }
    return result;
}

template <class T, class Context>
requires HasLazyObjectMap<T, Context>
void
LazyObjectImpl<T, Context>::
set(String key, Value value)
{
    overlay_.set(std::move(key), std::move(value));
}

template <class T, class Context>
requires HasLazyObjectMap<T, Context>
bool
LazyObjectImpl<T, Context>::
visit(std::function<bool(String, Value)> fn) const
{
    bool visitMore = true;
    detail::LazyObjectIO io(
        [&visitMore, &fn, this](std::string_view name, auto const& value)
        {
            if (visitMore && !overlay_.exists(name))
            {
                if constexpr (HasValueFromWithContext<std::remove_cvref_t<decltype(value)>, Context>)
                {
                    visitMore = fn(name, dom::ValueFrom(value, context_));
                }
                else
                {
                    visitMore = fn(name, dom::ValueFrom(value));
                }
            }
        }, [&visitMore, &fn, this](std::string_view name, auto const& deferred)
        {
            if (visitMore && !overlay_.exists(name))
            {
                if constexpr (HasValueFromWithContext<std::remove_cvref_t<decltype(deferred())>, Context>)
                {
                    visitMore = fn(name, dom::ValueFrom(deferred(), context_));
                }
                else
                {
                    visitMore = fn(name, dom::ValueFrom(deferred()));
                }
            }
        });
    if constexpr (HasLazyObjectMapWithContext<T, Context>)
    {
        tag_invoke(LazyObjectMapTag{}, io, *underlying_, context_);
    }
    else
    {
        tag_invoke(LazyObjectMapTag{}, io, *underlying_);
    }
    return visitMore && overlay_.visit(fn);
}


/** Return a new dom::Object based on a lazy object implementation.

    @param obj The underlying object.
    @return A new dom::Object whose properties are the result of converting each property in the underlying object to a dom::Value.
*/
template <HasLazyObjectMapWithoutContext T>
Object
LazyObject(T const& obj)
{
    return newObject<LazyObjectImpl<T>>(obj);
}

/** Return a new dom::Object based on a transformed lazy array implementation.

    @param arr The underlying range of elements.
    @param context The context used to convert each element to a dom::Value.
    @return A new dom::Array whose elements are the result of converting each element in the underlying range using the specified context.
*/
template <class T, class Context>
requires HasLazyObjectMap<T, Context>
Object
LazyObject(T const& arr, Context const& context)
{
    return newObject<LazyObjectImpl<T, Context>>(arr, context);
}

} // clang::mrdocs::dom

#endif // MRDOCS_API_DOM_LAZYOBJECT_HPP
