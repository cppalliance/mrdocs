//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_VISITOR_HPP
#define MRDOCS_API_SUPPORT_VISITOR_HPP

#include <mrdocs/Support/TypeTraits.hpp>
#include <tuple>
#include <utility>

namespace clang::mrdocs {

/** A visitor for a type

    This class is used to implement the visitor
    pattern. It stores a reference to an object
    of type `Base`, and a function object `Fn`
    which is called with the derived type as
    the first argument, followed by `Args`.

    The visitor is constructed with the object
    to visit, the function object, and the
    arguments to pass to the function object.

    The method `visit` is a template which
    accepts a derived type of `Base`. It calls
    the function object with the derived type
    as the first argument, followed by the
    arguments passed to the constructor.

    @tparam Base The base type of the object
    @tparam Fn The function object type
    @tparam Args The argument types
 */
template<
    typename Base,
    typename Fn,
    typename... Args>
class Visitor
{
    Base&& obj_;
    Fn&& fn_;
    std::tuple<Args&&...> args_;

public:
    /** Constructor

        @param obj The object to visit
        @param fn The function object to call
        @param args The arguments to pass to the function object
     */
    Visitor(Base&& obj, Fn&& fn, Args&&... args)
        : obj_(std::forward<Base>(obj))
        , fn_(std::forward<Fn>(fn))
        , args_(std::forward<Args>(args)...)
    {
    }

    /** Visit a derived type

        This method calls the function object with
        the derived type as the first argument,
        followed by the arguments passed to the
        constructor.

        @tparam Derived The derived type to visit
        @return The result of calling the function object
     */
    template <std::derived_from<std::remove_cvref_t<Base>> Derived>
    decltype(auto)
    visit()
    {
        return std::apply(
            std::forward<Fn>(fn_),
            std::tuple_cat(
                std::forward_as_tuple(
                    static_cast<add_cvref_from_t<
                        Base, Derived>&&>(obj_)),
                std::move(args_)));
    }
};

/** Make a visitor for a base type

    The returned visitor is an object with a template
    method `visit` which can be called with a derived
    type of the object being visited.

    The visitor stores the arguments `args` passed to
    this function, and its method `visit` calls the
    function `fn` with the derived type as the first
    argument, followed by `args`.

    @param obj The object to visit
    @param fn The function object to call
    @param args The arguments to pass to the function object
    @return The common return type of `fn` when called
            with a derived type of `obj` and `args`
 */
template<
    typename BaseTy,
    typename ObjectTy,
    typename FnTy,
    typename... ArgsTy>
auto
makeVisitor(
    ObjectTy&& obj,
    FnTy&& fn,
    ArgsTy&&... args)
{
    using VisitorTy = Visitor<
        add_cvref_from_t<ObjectTy, BaseTy>,
        FnTy, ArgsTy...>;
    return VisitorTy(
        std::forward<ObjectTy>(obj),
        std::forward<FnTy>(fn),
        std::forward<ArgsTy>(args)...);
}

} // clang::mrdocs

#endif // MRDOCS_API_SUPPORT_VISITOR_HPP
