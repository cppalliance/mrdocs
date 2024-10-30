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
    class GetterIO
    {
        std::string_view key;
        Value result;
    public:
        explicit
        GetterIO(std::string_view key)
            : key(key) {}

        template <class T>
        requires std::constructible_from<Value, T>
        void
        map(std::string_view name, T const& value)
        {
            if (result.isUndefined() && name == key)
            {
                this->result = Value(value);
            }
        }

        template <class T>
        void
        defer(std::string_view name, T const& deferred)
        {
            using R = std::invoke_result_t<T>;
            if constexpr (std::constructible_from<Value, R>)
            {
                if (result.isUndefined() && name == key)
                {
                    this->result = deferred();
                }
            }
        }

        Value
        get()
        {
            return std::move(result);
        }
    };
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
    detail::GetterIO io{key};
    traits_.map(io, *underlying_);
    return io.get();
}

template <HasMappingTraits T>
void
LazyObjectImpl<T>::
set(String key, Value value)
{
    overlay_.set(std::move(key), std::move(value));
}

namespace detail
{
    class VisitIO
    {
        std::function<bool(String, Value)> fn;
        Object const& overlay;
        bool continueVisiting = true;
    public:
        explicit
            VisitIO(std::function<bool(String, Value)> fn, Object const& overlay)
            : fn(fn)
            , overlay(overlay) {}

        template <class T>
        void
        map(std::string_view name, T const& value)
        {
            if (continueVisiting && !overlay.exists(name))
            {
                continueVisiting = fn(name, Value(value));
            }
        }

        template <class T>
        void
        defer(std::string_view name, T const& deferred)
        {
            if (continueVisiting && !overlay.exists(name))
            {
                continueVisiting = fn(name, deferred());
            }
        }

        bool
        get()
        {
            return continueVisiting;
        }
    };
}

template <HasMappingTraits T>
bool
LazyObjectImpl<T>::
visit(std::function<bool(String, Value)> fn) const
{
    detail::VisitIO io{fn, overlay_};
    traits_.map(io, *underlying_);
    return io.get() && overlay_.visit(fn);
}

namespace detail
{
    class SizeIO
    {
        Object const& overlay;
        std::size_t result = 0;
    public:
        explicit
        SizeIO(Object const& overlay)
            : overlay(overlay) {}

        template <class T>
        void
        map(std::string_view name, T const&)
        {
            this->result += !overlay.exists(name);
        }

        template <class T>
        void
        defer(std::string_view name, T const&)
        {
            this->result += !overlay.exists(name);
        }

        std::size_t
        get()
        {
            return result + overlay.size();
        }
    };
}

template <HasMappingTraits T>
std::size_t
LazyObjectImpl<T>::
size() const
{
    detail::SizeIO io{overlay_};
    traits_.map(io, *underlying_);
    return io.get();
}

namespace detail
{
    class ExistsIO
    {
        std::string_view key;
        bool result = false;
    public:
        explicit
        ExistsIO(std::string_view key)
            : key(key) {}

        template <class T>
        void
        map(std::string_view name, T const&)
        {
            if (!result && name == key)
            {
                this->result = true;
            }
        }

        template <class T>
        void
        defer(std::string_view name, T const&)
        {
            if (!result && name == key)
            {
                this->result = true;
            }
        }

        bool
        get()
        {
            return result;
        }
    };
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
    detail::ExistsIO io{key};
    traits_.map(io, *underlying_);
    return io.get();
}

} // dom
} // mrdocs
} // clang

#endif
