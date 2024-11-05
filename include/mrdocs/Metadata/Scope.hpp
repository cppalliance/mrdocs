//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SCOPE_HPP
#define MRDOCS_API_METADATA_SCOPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <unordered_map>
#include <string>
#include <vector>

namespace clang {
namespace mrdocs {

/** Stores the members and lookups for an Info.

    Members are the symbols that are directly
    contained in the scope of the Info. For instance,
    the members of a namespace are the symbols
    declared in the namespace. The members of a
    class are the symbols and functions declared
    in the class.

    The Lookups are the symbols that are accessible
    from the scope of the Info. For instance, the
    Lookups["foo"] of a namespace are the symbols
    declared as "foo" in the namespace.

    This Info class can be used as a base class
    for other Info classes, such as NamespaceInfo,
    ClassInfo, that represent scopes. This class
    can also be used with composition, such as in
    @ref Interface to represent different scopes of
    the same class (such as member and static overloads).

*/
struct ScopeInfo
{
    /** The members of this scope.
    */
    std::vector<SymbolID> Members;

    /** The lookup table for this scope.
    */
    std::unordered_map<
        std::string,
        std::vector<SymbolID>> Lookups;
};

/** Get a dom::Array of overloads for a scope

    This function takes a ScopeInfo, such as
    Namespace or Record, and returns a dom::Array
    of overloads in this scope using the DomCorpus
    to resolve the SymbolIDs in this scope.

    If the symbol is not overloaded, the
    symbol is included in the array.

    When the symbol is overloaded, the OverloadSet
    is included in the array.

    This function makes no distinction between
    overloads with different access specifiers.

    Instead, we need to traverse the overloads and
    generate the data whenever the information
    is requested in the handlebars templates
    via the LazyObject.
 */
MRDOCS_DECL
dom::Array
generateScopeOverloadsArray(
    ScopeInfo const& I,
    DomCorpus const& domCorpus);

} // mrdocs
} // clang

#endif
