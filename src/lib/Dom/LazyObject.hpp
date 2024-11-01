//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_DOM_LAZY_OBJECT_HPP
#define MRDOCS_LIB_DOM_LAZY_OBJECT_HPP

#include "mrdocs/Dom.hpp"
#include "mrdocs/Platform.hpp"
#include "mrdocs/Support/Error.hpp"
#include <string_view>

namespace clang {
namespace mrdocs {
namespace dom {

/** Mapping traits to convert types into dom::Object.

    This class should be specialized by any type that needs to be converted
    to/from a @ref dom::Object.  For example:

    @code
    template<>
    struct MappingTraits<MyStruct> {
        template <class IO>
        static void map(IO &io, MyStruct const& s)
        {
            io.map("name", s.name);
            io.map("size", s.size);
            io.map("age",  s.age);
        }
    };
    @endcode
 */
template<class T>
struct MappingTraits {
  // void map(dom::IO &io, T &fields) const;
};

namespace detail
{
    /** A class representing an archetypal IO object.
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
}

/// Concept to determine if @ref MappingTraits is defined for a type T
template <class T>
concept HasMappingTraits = requires(detail::ArchetypalIO& io, T const& o)
{
    { std::declval<MappingTraits<T>>().map(io, o) } -> std::same_as<void>;
};

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
    the object Value is constructed.
    In practice, the object never takes any memory
    besides the pointer to the underlying object.

    The keys and values in the underlying object
    should be mapped using the MappingTraits<T> class.

    This class is typically useful for
    implementing objects that are expensive
    and have recursive dependencies, as these
    recursive dependencies can also be deferred.

*/
template <HasMappingTraits T>
class LazyObjectImpl : public ObjectImpl
{
    T const* underlying_;
    Object overlay_;
    [[no_unique_address]] MappingTraits<T> traits_{};

public:
    explicit
    LazyObjectImpl(T const& obj)
        requires std::constructible_from<MappingTraits<T>>
        : underlying_(&obj)
        , traits_{} {}

    explicit
    LazyObjectImpl(T const& obj, MappingTraits<T> traits)
        : underlying_(&obj)
        , traits_(std::move(traits)) {}

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

        template <HasStandaloneValueFrom T>
        void
        map(std::string_view name, T const& value)
        {
            mapFn(name, value);
        }

        template <class F>
        requires HasStandaloneValueFrom<std::invoke_result_t<F>>
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
}

template <HasMappingTraits T>
std::size_t
LazyObjectImpl<T>::
size() const
{
    std::size_t result;
    detail::LazyObjectIO io(
        [&result, this](std::string_view name, auto const& /* value or deferred */)
        {
            result += !overlay_.exists(name);
        });
    traits_.map(io, *underlying_);
    return result + overlay_.size();
}

template <HasMappingTraits T>
bool
LazyObjectImpl<T>::
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
    traits_.map(io, *underlying_);
    return result;
}


template <HasMappingTraits T>
Value
LazyObjectImpl<T>::
get(std::string_view key) const
{
    if (overlay_.exists(key))
    {
        return overlay_.get(key);
    }
    Value result;
    detail::LazyObjectIO io(
        [&result, key](std::string_view name, auto const& value)
        {
            if (result.isUndefined() && name == key)
            {
                ValueFrom(value, result);
            }
        }, [&result, key](std::string_view name, auto const& deferred)
        {
            if (result.isUndefined() && name == key)
            {
                ValueFrom(deferred(), result);
            }
        });
    traits_.map(io, *underlying_);
    return result;
}

template <HasMappingTraits T>
void
LazyObjectImpl<T>::
set(String key, Value value)
{
    overlay_.set(std::move(key), std::move(value));
}

template <HasMappingTraits T>
bool
LazyObjectImpl<T>::
visit(std::function<bool(String, Value)> fn) const
{
    bool visitMore = true;
    detail::LazyObjectIO io(
        [&visitMore, &fn, this](std::string_view name, auto const& value)
        {
            if (visitMore && !overlay_.exists(name))
            {
                visitMore = fn(name, dom::ValueFrom(value));
            }
        }, [&visitMore, &fn, this](std::string_view name, auto const& deferred)
        {
            if (visitMore && !overlay_.exists(name))
            {
                visitMore = fn(name, dom::ValueFrom(deferred()));
            }
        });
    traits_.map(io, *underlying_);
    return visitMore && overlay_.visit(fn);
}


} // dom
} // mrdocs
} // clang

#endif
