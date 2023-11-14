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
*/
struct ScopeInfo
{
	/** The members of this scope.
	*/
	std::vector<SymbolID> Members;

	/** The lookup table for this scope.
	*/
	std::unordered_map<std::string,
        std::vector<SymbolID>> Lookups;
};

} // mrdocs
} // clang

#endif
