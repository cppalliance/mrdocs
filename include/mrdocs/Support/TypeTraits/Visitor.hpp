//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_TYPETRAITS_VISITOR_HPP
#define MRDOCS_API_SUPPORT_TYPETRAITS_VISITOR_HPP

#include <mrdocs/Support/TypeTraits/TypeTraits.hpp>
#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <type_traits>
#include <utility>

namespace mrdocs {

namespace detail {

/** Match a runtime kind against a list of derived types and dispatch.

    Walks the derived types `Head, Tail...`, comparing `info.Kind`
    against each type's `kind_id` constant. On the match, `info` is
    downcast to that derived type, keeping its cv and reference
    qualifiers, and `fn` is called with the downcast object followed by
    `args`. The list is exhaustive for a well-formed object, so a value
    matching none is unreachable.

    @param info The object whose `Kind` selects the derived type.
    @param fn The function called with the downcast object and `args`.
    @param args Extra arguments forwarded to `fn`.
    @return The result of calling `fn`.
*/
template<class Head, class... Tail, class Info, class Fn, class... Args>
decltype(auto)
visitByKindId(Info& info, Fn&& fn, Args&&... args)
{
    if (info.Kind == Head::kind_id)
    {
        return std::forward<Fn>(fn)(
            static_cast<add_cvref_from_t<Info&, Head>>(info),
            std::forward<Args>(args)...);
    }
    if constexpr (sizeof...(Tail) > 0)
    {
        return visitByKindId<Tail...>(
            info, std::forward<Fn>(fn), std::forward<Args>(args)...);
    }
    else
    {
        MRDOCS_UNREACHABLE();
    }
}

} // namespace detail

/** Visit a polymorphic object by matching its kind to a derived type.

    Accepts either a polymorphic base registered with
    `MRDOCS_DESCRIBE_KINDS`, or one of its concrete kinds (a type carrying
    a `kind_id`).

    Given a base, it reads the concrete kinds from
    `describe::describe_kinds` and calls `fn` with the object downcast to
    the kind whose `kind_id` equals `info.Kind`, followed by `args`.

    Given a concrete kind, the type is already known statically, so there
    is nothing to dispatch: `fn` is called with `info` directly. This lets
    a caller that already holds a concrete node (e.g. the global
    `NamespaceSymbol`) pass it without casting to the base first.

    @param info The object to visit; when it is a base, `info.Kind`
        selects the derived type.
    @param fn The function called with the concrete object and `args`.
    @param args Extra arguments forwarded to `fn`.
    @return The result of calling `fn` with the concrete object.
*/
template<
    class Info,
    class Fn,
    class... Args>
    requires (describe::has_describe_kinds<std::remove_cvref_t<Info>>::value
        || requires { std::remove_cvref_t<Info>::kind_id; })
decltype(auto)
visit(Info& info, Fn&& fn, Args&&... args)
{
    using Base = std::remove_cvref_t<Info>;
    if constexpr (describe::has_describe_kinds<Base>::value)
    {
        return [&]<class... Kinds>(describe::list<Kinds...>) -> decltype(auto)
        {
            return detail::visitByKindId<typename Kinds::type...>(
                info, std::forward<Fn>(fn), std::forward<Args>(args)...);
        }(describe::describe_kinds<Base>{});
    }
    else
    {
        return std::forward<Fn>(fn)(info, std::forward<Args>(args)...);
    }
}

} // mrdocs

#endif // MRDOCS_API_SUPPORT_TYPETRAITS_VISITOR_HPP
