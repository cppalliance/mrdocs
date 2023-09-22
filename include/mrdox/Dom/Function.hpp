//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_FUNCTION_HPP
#define MRDOX_API_DOM_FUNCTION_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Dom/Kind.hpp>
#include <mrdox/Dom/String.hpp>
#include <mrdox/Support/Error.hpp>
#include <memory>

namespace clang {
namespace mrdox {
namespace dom {

class Array;
class Value;

//------------------------------------------------
//
// function_traits
//
//------------------------------------------------
template<typename F>
struct function_traits;

template<typename R, typename... Args>
struct function_traits<R(Args...)>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...)>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) volatile>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const volatile>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

// (missing permutations of ref-qualifier and cv-qualifier)

template<typename R, typename... Args>
struct function_traits<R(*)(Args...)>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

template<typename F>
    requires requires { &F::operator(); }
struct function_traits<F>
    : function_traits<decltype(&F::operator())>
{
};

template<typename F>
concept has_function_traits = requires {
    typename function_traits<F>::return_type;
    typename function_traits<F>::args_type;
};

template<typename F>
concept has_invoke_result_for_default_function_impl =
    // Return type is void or convertible to dom::Value
    std::convertible_to<typename function_traits<F>::return_type, Value> ||
    std::same_as<typename function_traits<F>::return_type, void>;

template<typename F>
concept has_function_args_for_default_function_impl =
    // All arguments are convertible to dom::Value
    (std::tuple_size_v<typename function_traits<F>::args_type> == 0 ||
        []<std::size_t... I>(std::index_sequence<I...>) {
        return
            (std::convertible_to<
                std::tuple_element_t<
                    I, typename function_traits<F>::args_type>,
                Value> && ...);
        }(std::make_index_sequence<std::tuple_size_v<typename function_traits<F>::args_type>>()));

template<typename F>
concept has_function_traits_for_default_function_impl =
    has_invoke_result_for_default_function_impl<F> &&
    has_function_args_for_default_function_impl<F>;


template<class F>
concept function_traits_convertible_to_value =
    has_function_traits<F> && has_function_traits_for_default_function_impl<F>;

//------------------------------------------------
//
// Function
//
//------------------------------------------------

class FunctionImpl;

template<class F>
class DefaultFunctionImpl;

class MRDOX_DECL
    Function
{
    using impl_type = std::shared_ptr<FunctionImpl>;
    impl_type impl_;

    explicit
    Function(
        std::shared_ptr<FunctionImpl> impl) noexcept
        : impl_(std::move(impl))
    {
        MRDOX_ASSERT(impl_);
    }

public:
    /** Destructor.
    */
    ~Function();

    /** Constructor.

        A default-constructed function has this
        equivalent implementation:
        @code
        Value f()
        {
            return nullptr;
        }
        @endcode
    */
    Function() noexcept;

    /** Constructor.

        Ownership of the function is tranferred.
        The moved-from object behaves as if default
        constructed.
    */
    Function(Function&&) noexcept;

    /** Constructor.

        The newly constructed object acquires shared
        ownership of the function.
    */
    Function(Function const&) noexcept;

    template<class F>
    requires
        function_traits_convertible_to_value<std::decay_t<F>>
    Function(F const& f)
        : Function(std::make_shared<
            DefaultFunctionImpl<
                std::decay_t<F>>>(f))
    {}

    /** Assignment.

        Ownership of the function is tranferred,
        and ownership of the previous function is
        released. The moved-from object behaves as
        if default constructed.
    */
    Function& operator=(Function&&) noexcept;

    /** Assignment.

        This acquires shared ownership of the
        function. Ownership of the previous function
        is removed.
    */
    Function& operator=(Function const&) noexcept;

    /** Return the implementation used by this object.
    */
    auto
    impl() const noexcept ->
        impl_type const&
    {
        return impl_;
    }

    /** Return the type key.
    */
    char const* type_key() const noexcept;

    /** Invoke the function.
    */
    Expected<Value>
    call(Array const& args) const;

    /** Invoke the function.
    */
    template<class... Args>
    Value operator()(Args&&... args) const;

    /** Swap two objects.
    */
    void swap(Function& other) noexcept
    {
        std::swap(impl_, other.impl_);
    }

    /** Swap two objects.
    */
    friend void swap(Function &lhs, Function& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    template<class T, class... Args>
    requires std::derived_from<T, FunctionImpl>
    friend Function newFunction(Args&&... args);
};

//------------------------------------------------
//
// FunctionImpl
//
//------------------------------------------------

class MRDOX_DECL
    FunctionImpl
{
public:
    /** Destructor.
    */
    virtual ~FunctionImpl() = default;

    /** Return the type key of the implementation.
    */
    virtual char const* type_key() const noexcept;

    /** Invoke the function.
    */
    virtual Expected<Value> call(Array const& args) const = 0;
};

/** Return a new function using a custom implementation.
*/
template<class T, class... Args>
requires std::derived_from<T, FunctionImpl>
Function
newFunction(Args&&... args)
{
    return Function(std::make_shared<T>(
        std::forward<Args>(args)...));
}

//------------------------------------------------
//
// DefaultFunctionImpl
//
//------------------------------------------------

template<class F>
class DefaultFunctionImpl : public FunctionImpl
{
    F f_;

public:
    using return_type = typename function_traits<F>::return_type;
    using args_type = typename function_traits<F>::args_type;

    template<class U>
    DefaultFunctionImpl(U&& u)
        : f_(std::forward<U>(u))
    {
    }

    char const* type_key() const noexcept override
    {
        return "DefaultFunctionImpl";
    }

    Expected<Value>
    call(Array const& args) const override;

private:
    template<std::size_t... I>
    Expected<Value> call_impl(
        Array const& args,
        std::index_sequence<I...>) const;
};

template<class F>
Function makeInvocable(F&& f)
{
    return newFunction<DefaultFunctionImpl<std::decay_t<F>>>(
        std::forward<F>(f));
}

template<class F>
class VariadicFunctionImpl : public FunctionImpl
{
    F f_;

public:
    using return_type = typename function_traits<F>::return_type;
    using args_type = typename function_traits<F>::args_type;

    template<class U>
    VariadicFunctionImpl(U&& u)
        : f_(std::forward<U>(u))
    {
    }

    char const* type_key() const noexcept override
    {
        return "VariadicFunctionImpl";
    }

    Expected<Value>
    call(Array const& args) const override;
};

template<class F>
requires std::invocable<F, Array const&>
Function makeVariadicInvocable(F&& f)
{
    return newFunction<VariadicFunctionImpl<std::decay_t<F>>>(
        std::forward<F>(f));
}

} // dom
} // mrdox
} // clang

// This is here because of circular references
#include <mrdox/Dom/Value.hpp>

#endif
