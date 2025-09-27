//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_OBJECT_IPP
#define MRDOCS_API_DOM_OBJECT_IPP


namespace mrdocs {
namespace dom {

//------------------------------------------------
//
// Object::value_type
//
//------------------------------------------------

struct Object::value_type
{
    String key;
    Value value;

    value_type() = default;

    value_type(
        String key_,
        Value value_) noexcept
        : key(std::move(key_))
        , value(std::move(value_))
    {
    }

    template<class U>
    requires std::constructible_from<
        std::pair<String, Value>, U>
    value_type(
        U&& u)
        : value_type(
            [&u]
            {
                std::pair<String, Value> p(
                    std::forward<U>(u));
                return value_type(
                    std::move(p).first,
                    std::move(p).second);
            })
    {
    }

    value_type* operator->() noexcept
    {
        return this;
    }

    value_type const* operator->() const noexcept
    {
        return this;
    }
};

//------------------------------------------------
//
// Object
//
//------------------------------------------------

inline char const* Object::type_key() const noexcept
{
    return impl_->type_key();
}

inline bool Object::empty() const
{
    return size() == 0;
}

inline std::size_t Object::size() const
{
    return impl_->size();
}

inline auto Object::get(std::string_view key) const -> Value
{
    return impl_->get(key);
}

inline auto Object::at(std::string_view key) const -> Value
{
    return this->get(key);
}

inline void Object::set(String key, Value value) const
{
    impl_->set(std::move(key), std::move(value));
}

template <class F>
requires
    std::invocable<F, String, Value> &&
    std::same_as<std::invoke_result_t<F, String, Value>, bool>
bool Object::visit(F&& fn) const
{
    return impl_->visit(std::forward<F>(fn));
}

template <class F>
requires
    std::invocable<F, String, Value> &&
    std::same_as<std::invoke_result_t<F, String, Value>, void>
void Object::visit(F&& fn) const
{
    impl_->visit([f=std::forward<F>(fn)](String k, Value v) {
        f(std::move(k), std::move(v));
        return true;
    });
}

template <class F>
requires
    std::invocable<F, String, Value> &&
    mrdocs::detail::isExpected<std::invoke_result_t<F, String, Value>>
Expected<void, typename std::invoke_result_t<F, String, Value>::error_type>
Object::visit(F&& fn) const
{
    using E = typename std::invoke_result_t<F, String, Value>::error_type;
    Expected<void, E> res;
    impl_->visit([f=std::forward<F>(fn), &res](String k, Value v) {
        Expected<void, E> exp = f(std::move(k), std::move(v));
        if (!exp.has_value())
        {
            res = Unexpected(exp.error());
            return false;
        }
        return true;
    });
    return res;
}

} // dom
} // mrdocs


//------------------------------------------------

template<>
struct std::formatter<mrdocs::dom::Object>
    : std::formatter<std::string>
{
    template <class FmtContext>
    auto format(
        mrdocs::dom::Object const& value,
        FmtContext& ctx) const
    {
        return std::formatter<std::string>::format(
            toString(value), ctx);
    }
};

#endif
