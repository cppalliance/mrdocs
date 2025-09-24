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

#ifndef MRDOCS_API_METADATA_INFO_RECORDTRANCHE_HPP
#define MRDOCS_API_METADATA_INFO_RECORDTRANCHE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Info/SymbolID.hpp>
#include <vector>

namespace clang::mrdocs {

/** A group of members that have the same access specifier.

    This struct represents a collection of symbols that share
    the same access specifier within a record.

    It includes one vector for each info type allowed in a
    record, and individual vectors for static functions, types,
    and function overloads.
*/
struct RecordTranche
{
    std::vector<SymbolID> NamespaceAliases;
    std::vector<SymbolID> Typedefs;
    std::vector<SymbolID> Records;
    std::vector<SymbolID> Enums;
    std::vector<SymbolID> Functions;
    std::vector<SymbolID> StaticFunctions;
    std::vector<SymbolID> Variables;
    std::vector<SymbolID> StaticVariables;
    std::vector<SymbolID> Concepts;
    std::vector<SymbolID> Guides;
    std::vector<SymbolID> Usings;
};

MRDOCS_DECL
void
merge(RecordTranche& I, RecordTranche&& Other);

inline
auto
allMembers(RecordTranche const& T)
{
    // This is a trick to emulate views::concat in C++20
    return std::views::transform(
        std::views::iota(0, 11),
        [&T](int const i) -> auto const&
        {
            switch (i) {
                case 0: return T.NamespaceAliases;
                case 1: return T.Typedefs;
                case 2: return T.Records;
                case 3: return T.Enums;
                case 4: return T.Functions;
                case 5: return T.StaticFunctions;
                case 6: return T.Variables;
                case 7: return T.StaticVariables;
                case 8: return T.Concepts;
                case 9: return T.Guides;
                case 10: return T.Usings;
                default: throw std::out_of_range("Invalid index");
            }
        }
    ) | std::ranges::views::join;
}

/** Map a RecordTranche to a dom::Object.

    @param io The output parameter to receive the dom::Object.
    @param I The RecordTranche to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordTranche const& I,
    DomCorpus const* domCorpus)
{
    io.map("namespaceAliases", dom::LazyArray(I.NamespaceAliases, domCorpus));
    io.map("typedefs", dom::LazyArray(I.Typedefs, domCorpus));
    io.map("records", dom::LazyArray(I.Records, domCorpus));
    io.map("enums", dom::LazyArray(I.Enums, domCorpus));
    io.map("functions", dom::LazyArray(I.Functions, domCorpus));
    io.map("staticFunctions", dom::LazyArray(I.StaticFunctions, domCorpus));
    io.map("variables", dom::LazyArray(I.Variables, domCorpus));
    io.map("staticVariables", dom::LazyArray(I.StaticVariables, domCorpus));
    io.map("concepts", dom::LazyArray(I.Concepts, domCorpus));
    io.map("guides", dom::LazyArray(I.Guides, domCorpus));
    io.map("usings", dom::LazyArray(I.Usings, domCorpus));
}

/** Map the RecordTranche to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordTranche const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}


} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_RECORDTRANCHE_HPP
