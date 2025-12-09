//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_NAMESPACE_HPP
#define MRDOCS_API_METADATA_SYMBOL_NAMESPACE_HPP

#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
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

    /** Compare tranches field-by-field.
    */
    auto operator<=>(NamespaceTranche const&) const = default;
};

/** Merge two tranches, appending members from the right-hand side.
*/
MRDOCS_DECL
void
merge(NamespaceTranche& I, NamespaceTranche&& Other);

/** Join all tranche member lists into a single view.
    @return Lazy view spanning every category stored in the tranche.
*/
inline
auto
allMembers(NamespaceTranche const& T)
{
    // This is a trick to emulate views::concat in C++20
    return std::views::transform(
        std::views::iota(0, 10),
        [&T](int const i) -> auto const& {
            switch (i) {
                case 0: return T.Namespaces;
                case 1: return T.NamespaceAliases;
                case 2: return T.Typedefs;
                case 3: return T.Records;
                case 4: return T.Enums;
                case 5: return T.Functions;
                case 6: return T.Variables;
                case 7: return T.Concepts;
                case 8: return T.Guides;
                case 9: return T.Usings;
                default: throw std::out_of_range("Invalid index");
            }
        }
    ) | std::ranges::views::join;
}

/** Map a NamespaceTranche to a dom::Object.

    @param io The IO object to use for mapping.
    @param I The NamespaceTranche to map.
    @param domCorpus The DomCorpus used to create the DOM values.
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NamespaceTranche const& I,
    DomCorpus const* domCorpus);

/** Map the NamespaceTranche to a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NamespaceTranche const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
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
    std::strong_ordering
    operator<=>(NamespaceSymbol const&) const;
};

/** Merge two namespaces, keeping existing members stable.
*/
MRDOCS_DECL
void
merge(NamespaceSymbol& I, NamespaceSymbol&& Other);

/** View all members of the namespace across tranches.
    @return Lazy view across every member bucket.
*/
inline
auto
allMembers(NamespaceSymbol const& T)
{
    return allMembers(T.Members);
}

/** Map a NamespaceSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The NamespaceSymbol to map.
    @param domCorpus The DomCorpus used to create
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    NamespaceSymbol const& I,
    DomCorpus const* domCorpus);

/** Map the NamespaceSymbol to a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NamespaceSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_NAMESPACE_HPP
