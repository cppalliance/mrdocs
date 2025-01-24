//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_OVERLOADS_HPP
#define MRDOCS_API_METADATA_OVERLOADS_HPP

#include <span>
#include <mrdocs/Platform.hpp>

namespace clang::mrdocs {

/** Represents a set of function overloads.

    Each `ScopeInfo`, such as `NamespaceInfo` and `RecordInfo`,
    contains a list of member IDs that are part of that scope
    and a lookup map of symbols with the same name that are accessible
    from that scope.

    By collecting the symbols for a given name from the lookup tables
    of all scopes in the namespace, we can create an `OverloadSet`.

    Besides the members of that scope, the `OverloadSet` also contains
    the original namespace and the parent of the overload set. This
    makes the OverloadSet similar to other `Info` classes, but it
    is not part of the corpus and is not directly accessible from
    the corpus: it needs to be constructed from the corpus.

 */
struct OverloadSet
{
    /// The name of the overload set.
    std::string_view Name;

    /// The parent symbol ID.
    SymbolID Parent;

    /// The members of the overload set.
    std::span<const SymbolID> Members;

    /**
     * @brief Constructs an OverloadSet.
     *
     * @param name The name of the overload set.
     * @param parent The parent symbol ID.
     * @param ns The namespace of the overload set.
     * @param members The members of the overload set.
     */
    OverloadSet(
        std::string_view name,
        const SymbolID& parent,
        std::span<const SymbolID> members)
        : Name(name)
        , Parent(parent)
        , Members(members)
    {
    }
};

template<
    class Fn,
    class... Args>
decltype(auto)
visit(
    const OverloadSet& overloads,
    Fn&& fn,
    Args&&... args)
{
    return std::forward<Fn>(fn)(overloads,
        std::forward<Args>(args)...);
}

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    OverloadSet const& overloads,
    DomCorpus const* domCorpus);

} // clang::mrdocs

#endif
