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

/** The members of a Namespace
 */
struct NamespaceTranche {
    std::vector<SymbolID> Namespaces;
    std::vector<SymbolID> NamespaceAliases;
    std::vector<SymbolID> Typedefs;
    std::vector<SymbolID> Records;
    std::vector<SymbolID> Enums;
    std::vector<SymbolID> Functions;
    std::vector<SymbolID> Variables;
    std::vector<SymbolID> Concepts;
    std::vector<SymbolID> Guides;
    std::vector<SymbolID> Usings;

    auto operator<=>(NamespaceTranche const&) const = default;
};

MRDOCS_DECL
void
merge(NamespaceTranche& I, NamespaceTranche&& Other);

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
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NamespaceTranche const& I,
    DomCorpus const* domCorpus)
{
    io.map("namespaces", dom::LazyArray(I.Namespaces, domCorpus));
    io.map("namespaceAliases", dom::LazyArray(I.NamespaceAliases, domCorpus));
    io.map("typedefs", dom::LazyArray(I.Typedefs, domCorpus));
    io.map("records", dom::LazyArray(I.Records, domCorpus));
    io.map("enums", dom::LazyArray(I.Enums, domCorpus));
    io.map("functions", dom::LazyArray(I.Functions, domCorpus));
    io.map("variables", dom::LazyArray(I.Variables, domCorpus));
    io.map("concepts", dom::LazyArray(I.Concepts, domCorpus));
    io.map("guides", dom::LazyArray(I.Guides, domCorpus));
    io.map("usings", dom::LazyArray(I.Usings, domCorpus));
}

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

/** Describes a namespace.
*/
struct NamespaceSymbol final
    : SymbolCommonBase<SymbolKind::Namespace>
{
    bool IsInline = false;
    bool IsAnonymous = false;

    /** Namespaces nominated by using-directives.
    */
    std::vector<struct Name> UsingDirectives;

    /** The members of this namespace.
    */
    NamespaceTranche Members;

    explicit NamespaceSymbol(SymbolID const &ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(NamespaceSymbol const&) const;
};

MRDOCS_DECL
void merge(NamespaceSymbol& I, NamespaceSymbol&& Other);

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
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    io.map("isInline", I.IsInline);
    io.map("isAnonymous", I.IsAnonymous);
    io.map("members", I.Members);
    io.map("usingDirectives", dom::LazyArray(I.UsingDirectives, domCorpus));
}

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
