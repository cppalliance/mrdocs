//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_NAMESPACE_HPP
#define MRDOCS_API_METADATA_SYMBOL_NAMESPACE_HPP

#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <ranges>
#include <vector>

namespace mrdocs {

/** Buckets the members that appear inside a namespace.
*/
struct NamespaceTranche {
    /** Nested namespaces.
    */
    std::vector<SymbolID> Namespaces;
    /** Namespace aliases declared here.
    */
    std::vector<SymbolID> NamespaceAliases;
    /** Typedef or using declarations.
    */
    std::vector<SymbolID> Typedefs;
    /** Record types (classes/structs).
    */
    std::vector<SymbolID> Records;
    /** Enumerations.
    */
    std::vector<SymbolID> Enums;
    /** Functions and overload sets.
    */
    std::vector<SymbolID> Functions;
    /** Variables.
    */
    std::vector<SymbolID> Variables;
    /** Concepts.
    */
    std::vector<SymbolID> Concepts;
    /** Deduction guides.
    */
    std::vector<SymbolID> Guides;
    /** Using-declarations that introduce members.
    */
    std::vector<SymbolID> Usings;

    /** Preprocessor macros.

        Only the global namespace's tranche carries these:
        macros have no C++ scope, so they are collected at the
        top level rather than under any named namespace.
    */
    std::vector<SymbolID> Macros;

};

MRDOCS_DESCRIBE_STRUCT(
    NamespaceTranche,
    (),
    (Namespaces, NamespaceAliases, Typedefs, Records, Enums,
     Functions, Variables, Concepts, Guides, Usings, Macros)
)

/** Join all tranche member lists into a single view.
    @return Lazy view spanning every category stored in the tranche.
*/
inline
auto
allMembers(NamespaceTranche const& T)
{
    // Concatenate every member list (emulating C++26 views::concat).
    // The lists are discovered by reflection, so adding a member to
    // NamespaceTranche extends this automatically, no switch to keep
    // in sync.
    static constexpr auto members =
        describe::memberPointers<NamespaceTranche>();
    return members
        | std::views::transform(
            [&T](auto const p) -> auto const& { return T.*p; })
        | std::ranges::views::join;
}

/** Describes a namespace and its members.
*/
struct NamespaceSymbol final
    : SymbolCommonBase<SymbolKind::Namespace>
{
    /** Whether this declaration is inline.
    */
    bool IsInline = false;
    /** Whether this represents an unnamed namespace.
    */
    bool IsAnonymous = false;

    /** Namespaces nominated by using-directives.
    */
    std::vector<struct Name> UsingDirectives;

    /** The members of this namespace.
    */
    NamespaceTranche Members;

    /** Create a namespace symbol bound to an ID.
    */
    explicit
    NamespaceSymbol(SymbolID const &ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    /** Compare namespaces by attributes and member lists.
    */
    MRDOCS_DECL
    std::strong_ordering
    operator<=>(NamespaceSymbol const&) const;
};

MRDOCS_DESCRIBE_STRUCT(
    NamespaceSymbol,
    (SymbolCommonBase<SymbolKind::Namespace>),
    (IsInline, IsAnonymous, UsingDirectives, Members)
)

/** View all members of the namespace across tranches.
    @return Lazy view across every member bucket.
*/
inline
auto
allMembers(NamespaceSymbol const& T)
{
    return allMembers(T.Members);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_NAMESPACE_HPP
