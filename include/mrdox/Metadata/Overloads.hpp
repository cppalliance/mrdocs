//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_OVERLOADS_HPP
#define MRDOX_API_METADATA_OVERLOADS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <span>
#include <string_view>

namespace clang {
namespace mrdox {

class Corpus;

struct OverloadInfo
{
    /** The parent namespace or record.
    */
    Info const* Parent;

    /** The name for this set of functions.
    */
    std::string_view Name;

    /** The list of overloads.
    */
    std::span<FunctionInfo const*> Functions;
};

class MRDOX_VISIBLE
    NamespaceOverloads
{
public:
    std::vector<OverloadInfo> list;

    /** Constructor.

        @par Complexity
        `O(N * log(N))` in `data.size()`.
    */
    MRDOX_DECL
    NamespaceOverloads(
        NamespaceInfo const& I,
        std::vector<FunctionInfo const*> data);

private:
    std::vector<FunctionInfo const*> data_;
};

/** Create an overload set for all functions in a namespace.

    This function organizes all functions in the
    specified list into an overload set. The top
    level set is sorted alphabetically using a
    display sort.

    @return The overload set.

    @param list The list of function references to use.
*/
MRDOX_DECL
NamespaceOverloads
makeNamespaceOverloads(
    NamespaceInfo const& I,
    Corpus const& corpus);

} // mrdox
} // clang

#endif
