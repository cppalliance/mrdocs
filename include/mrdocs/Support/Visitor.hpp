//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TOOL_SUPPORT_VISITOR_HPP
#define MRDOCS_TOOL_SUPPORT_VISITOR_HPP

#include <mrdocs/Support/TypeTraits.hpp>
#include <utility>
#include <tuple>

namespace clang {
namespace mrdocs {

template<
    typename Base,
    typename Fn,
    typename... Args>
class Visitor
{
    Base&& obj_;
    Fn&& fn_;
    std::tuple<Args&&...> args_;

    template<typename Derived>
    decltype(auto)
    getAs()
    {
        return static_cast<add_cvref_from_t<
            Base, Derived>&&>(obj_);
    }

public:
    Visitor(Base&& obj, Fn&& fn, Args&&... args)
        : obj_(std::forward<Base>(obj))
        , fn_(std::forward<Fn>(fn))
        , args_(std::forward<Args>(args)...)
    {
    }

    template<typename Derived>
        requires std::derived_from<Derived,
            std::remove_cvref_t<Base>>
    decltype(auto)
    visit()
    {
        return std::apply(std::forward<Fn>(fn_), std::tuple_cat(
            std::forward_as_tuple(getAs<Derived>()), args_));
    }
};

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
        FnTy&&, ArgsTy&&...>;
    return VisitorTy(std::forward<ObjectTy>(obj),
        std::forward<FnTy>(fn),
        std::forward<ArgsTy>(args)...);
}

} // mrdocs
} // clang

#endif
