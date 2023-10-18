//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_FUNCTION_IPP
#define MRDOX_API_DOM_FUNCTION_IPP

namespace clang {
namespace mrdox {
namespace dom {

inline char const* Function::type_key() const noexcept
{
    return impl_->type_key();
}

inline
Expected<Value>
Function::
call(Array const& args) const
{
    return impl_->call(args);
}

template<class... Args>
Value
Function::
operator()(Args&&... args) const
{
    auto exp = try_invoke(std::forward<Args>(args)...);
    if (exp)
    {
        return *exp;
    }
    throw Exception(std::move(exp.error()));
}

template<class... Args>
Expected<Value>
Function::
try_invoke(Args&&... args) const
{
    typename Array::storage_type elements;
    elements.reserve(sizeof...(Args));
    if constexpr (sizeof...(Args) > 0)
    {
        [[maybe_unused]] int dummy[] = {
            (elements.emplace_back(
                 std::forward<Args>(args)), 0)... };
    }
    return call(newArray<DefaultArrayImpl>(
        std::move(elements)));
}

//------------------------------------------------

template<class T>
struct arg_type;

template<>
struct arg_type<String>
{
    static String const& get(Value const& value)
    {
        return value.getString();
    }
};

template<>
struct arg_type<Array>
{
    static Array const& get(Value const& value)
    {
        return value.getArray();
    }
};

template<>
struct arg_type<Object>
{
    static Object const& get(Value const& value)
    {
        return value.getObject();
    }
};

template<>
struct arg_type<Function>
{
    static Function const& get(Value const& value)
    {
        return value.getFunction();
    }
};

template<>
struct arg_type<Value>
{
    static Value const& get(Value const& value) noexcept
    {
        return value;
    }
};

template<>
struct arg_type<bool>
{
    static bool get(Value const& value) noexcept
    {
        return value.isTruthy();
    }
};

template<class T>
requires std::is_integral_v<T>
struct arg_type<T>
{
    static T get(Value const& value)
    {
        switch(value.kind())
        {
        case Kind::Null:    return 0;
        case Kind::Boolean: return value.getBool();
        case Kind::Integer: return value.getInteger();
        default:
            Error("type mismatch").Throw();
        }
    }
};

template<class T>
requires StringLikeTy<T>
struct arg_type<T>
{
    static T get(Value const& value)
    {
        return value.getString().get();
    }
};

template<class F>
auto
DefaultFunctionImpl<F>::
call(Array const& args) const ->
    Expected<Value>
{
    return call_impl(args,
        std::make_index_sequence<
            std::tuple_size_v<args_type>>{});
}

template<class F>
template<std::size_t... I>
Expected<Value>
DefaultFunctionImpl<F>::
call_impl(
    Array const& args,
    std::index_sequence<I...>) const
{
    using R = decltype(
        f_(arg_type<std::decay_t<
            std::tuple_element_t<I, args_type> >
                >::get(args.get(I))...));
    if (args.size() < sizeof...(I))
    {
        Array clone;
        for (std::size_t i = 0; i < args.size(); ++i)
            clone.emplace_back(args.get(i));
        std::size_t const diff = sizeof...(I) - args.size();
        for (std::size_t i = 0; i < diff; ++i)
            clone.emplace_back(Value(Kind::Undefined));
        if constexpr (std::is_void_v<R>)
        {
            f_(arg_type<std::decay_t<
                std::tuple_element_t<I, args_type> >
                    >::get(clone.get(I))...);
            return Value(Kind::Undefined);
        }
        else if constexpr (std::same_as<R, Expected<void>>)
        {
            auto exp = f_(arg_type<std::decay_t<
                std::tuple_element_t<I, args_type> >
                    >::get(clone.get(I))...);
            if (!exp)
            {
                return Unexpected(exp.error());
            }
            return Value(Kind::Undefined);
        }
        else
        {
            return f_(arg_type<std::decay_t<
              std::tuple_element_t<I, args_type> >
                   >::get(clone.get(I))...);
        }
    }
    if constexpr (std::is_void_v<R>)
    {
        f_(arg_type<std::decay_t<
            std::tuple_element_t<I, args_type> >
                >::get(args.get(I))...);
        return Value(Kind::Undefined);
    }
    else if constexpr (std::same_as<R, Expected<void>>)
    {
        auto exp = f_(arg_type<std::decay_t<
            std::tuple_element_t<I, args_type> >
                >::get(args.get(I))...);
        if (!exp)
        {
            return Unexpected(exp.error());
        }
        return Value(Kind::Undefined);
    }
    else
    {
        return f_(arg_type<std::decay_t<
            std::tuple_element_t<I, args_type> >
                >::get(args.get(I))...);
    }
}

//------------------------------------------------

template<class F>
auto
VariadicFunctionImpl<F>::
call(Array const& args) const ->
    Expected<Value>
{
    using R = std::invoke_result_t<F, Array const&>;
    if constexpr (std::is_void_v<R>)
    {
        f_(args);
        return Value(Kind::Undefined);
    }
    else if constexpr (std::is_same_v<R, Expected<void>>)
    {
        auto exp = f_(args);
        if (!exp)
        {
            return Unexpected(exp.error());
        }
        return Value(Kind::Undefined);
    }
    else
    {
        return f_(args);
    }
}

} // dom
} // mrdox
} // clang

#endif
