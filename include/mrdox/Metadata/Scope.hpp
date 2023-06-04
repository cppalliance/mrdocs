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

#ifndef MRDOX_API_METADATA_SCOPE_HPP
#define MRDOX_API_METADATA_SCOPE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Reference.hpp>
#include <mrdox/Metadata/Typedef.hpp>
#include <mrdox/Metadata/Enum.hpp>
#include <vector>

namespace clang {
namespace mrdox {

/** A container for the declaration in a namespace.
*/
struct Scope
{
    bool isNamespaceScope = true;

    // Namespaces and Records are references because they will be properly
    // documented in their own info, while the entirety of Functions and Enums are
    // included here because they should not have separate documentation from
    // their scope.
    //
    // Namespaces are not syntactically valid as children of records, but making
    // this general for all possible container types reduces code complexity.
    std::vector<Reference> Namespaces;
    std::vector<Reference> Records;
    std::vector<Reference> Functions;
    std::vector<Reference> Typedefs;
    std::vector<Reference> Enums;
    std::vector<Reference> Vars;

    explicit
    Scope(
        bool isNamespaceScope_) noexcept
        : isNamespaceScope(isNamespaceScope_)
    {
    }
};

} // mrdox
} // clang

#endif
