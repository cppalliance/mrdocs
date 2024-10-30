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

} // mrdocs
} // clang

#endif
