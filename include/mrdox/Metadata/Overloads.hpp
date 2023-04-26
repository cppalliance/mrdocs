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

#ifndef MRDOX_METADATA_OVERLOADS_HPP
#define MRDOX_METADATA_OVERLOADS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/StringRef.h>
#include <span>
#include <string_view>

namespace clang {
namespace mrdox {

class Corpus;
struct Scope;

/** A list of overloads for one function name.
*/
struct Overloads
{
    llvm::StringRef name;
    std::vector<FunctionInfo const*> list;
};

/** A set of unique function names in a scope
*/
struct OverloadsSet
{
    /** The access control of this scope.
    */
    AccessSpecifier access;

    /** The list of function overloads in the scope.
    */
    std::vector<Overloads> list;
};

/** Create an overload set for all functions in a scope.
*/
MRDOX_DECL
OverloadsSet
makeOverloadsSet(
    Corpus const& corpus,
    Scope const& scope);

/** Create an overload set for all functions in a scope.
*/
MRDOX_DECL
OverloadsSet
makeOverloadsSet(
    Corpus const& corpus,
    Scope const& scope,
    AccessSpecifier access);

//------------------------------------------------

struct Overloads_
{
    /** The name for this set of functions.
    */
    std::string_view name;

    /** The list of overloads.
    */
    std::span<FunctionInfo const*> list;
};

class MRDOX_VISIBLE
    NamespaceOverloads
{
public:
    std::vector<Overloads_> list;

    /** Constructor.

        @par Complexity
        `O(N * log(N))` in `data.size()`.
    */
    MRDOX_DECL
    explicit
    NamespaceOverloads(
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
    std::vector<Reference> const& list,
    Corpus const& corpus);

} // mrdox
} // clang

#endif
