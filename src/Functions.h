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

#ifndef MRDOX_FUNCTIONS_HPP
#define MRDOX_FUNCTIONS_HPP

#include "Types.h"
#include <clang/Basic/Specifiers.h>
#include <vector>
#include <map>

namespace mrdox {

//------------------------------------------------

/** A list of overloads for a function.
*/
struct FunctionOverloads
{
    /// The name of the function.
    UnqualifiedName name;

    /// The list of overloads.
    FunctionInfos list;

    // Combine other into this.
    void merge(FunctionOverloads&& other);

    FunctionOverloads(
        FunctionOverloads&&) noexcept = default;
    FunctionOverloads& operator=(
        FunctionOverloads&&) noexcept = default;
    FunctionOverloads(
        clang::doc::FunctionInfo I);
};

//------------------------------------------------

/** A list of functions, possibly overloaded,
*/
using Functions = std::vector<FunctionOverloads>;

//------------------------------------------------

/** A list of functions in a scope.
*/
struct ScopedFunctions
{
    // Array of functions grouped by Access
    std::vector<Functions> overloads;

    void insert(clang::doc::FunctionInfo I);
    void merge(ScopedFunctions&& other);

    ScopedFunctions();
    ScopedFunctions(
        ScopedFunctions&&) noexcept = default;

private:
    auto
    find(
        Functions& v,
        llvm::StringRef name) noexcept ->
            Functions::iterator;

    auto
    find(
        clang::doc::FunctionInfo const& I) noexcept ->
        Functions::iterator;
};

//------------------------------------------------

} // mrdox

#endif
