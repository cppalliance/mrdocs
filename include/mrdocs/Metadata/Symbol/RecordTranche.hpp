//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_RECORDTRANCHE_HPP
#define MRDOCS_API_METADATA_SYMBOL_RECORDTRANCHE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <vector>

namespace mrdocs {

/** A group of members that have the same access specifier.

    This struct represents a collection of symbols that share
    the same access specifier within a record.

    It includes one vector for each info type allowed in a
    record, and individual vectors for static functions, types,
    and function overloads.
*/
struct RecordTranche
{
    /** Namespace aliases declared with this access.
    */
    std::vector<SymbolID> NamespaceAliases;
    /** Typedefs and using aliases.
    */
    std::vector<SymbolID> Typedefs;
    /** Nested records.
    */
    std::vector<SymbolID> Records;
    /** Enumerations.
    */
    std::vector<SymbolID> Enums;
    /** Member functions.
    */
    std::vector<SymbolID> Functions;
    /** Static member functions.
    */
    std::vector<SymbolID> StaticFunctions;
    /** Data members.
    */
    std::vector<SymbolID> Variables;
    /** Static data members.
    */
    std::vector<SymbolID> StaticVariables;
    /** Member concepts.
    */
    std::vector<SymbolID> Concepts;
    /** Deduction guides in the class scope.
    */
    std::vector<SymbolID> Guides;
    /** Using-declarations that pull members into the class.
    */
    std::vector<SymbolID> Usings;
};

MRDOCS_DESCRIBE_STRUCT(
    RecordTranche,
    (),
    (NamespaceAliases, Typedefs, Records, Enums, Functions,
     StaticFunctions, Variables, StaticVariables, Concepts, Guides, Usings)
)

/** Join every member list into a single view.
    @return Lazy view spanning all member categories.
*/
inline
auto
allMembers(RecordTranche const& T)
{
    // Concatenate every member list (emulating C++26 views::concat).
    // The lists are discovered by reflection, so adding a member to
    // RecordTranche extends this automatically, no switch to keep in
    // sync.
    static constexpr auto members =
        describe::memberPointers<RecordTranche>();
    return members
        | std::views::transform(
            [&T](auto const p) -> auto const& { return T.*p; })
        | std::ranges::views::join;
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_RECORDTRANCHE_HPP
