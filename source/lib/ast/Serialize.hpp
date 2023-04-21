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

#ifndef MRDOX_SOURCE_AST_SERIALIZE_HPP
#define MRDOX_SOURCE_AST_SERIALIZE_HPP

#include "clangASTComment.hpp"
#include "Bitcode.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/AST/AST.h>
#include <string>
#include <utility>
#include <vector>

/*  The serialization algorithm takes as input a
    typed AST node, and produces as output a pair
    of one or two Bitcode objects. A bitcode
    contains the metadata for a given symbol ID,
    serialized as bitcode.
*/

namespace clang {
namespace mrdox {

/** Holds the result of serializing a Decl.

    This can result in two bitcodes. One for
    the declaration itself, and possibly one
    for the parent which is referenced by the
    decl.
*/
using SerializeResult = std::pair<Bitcode, Bitcode>;

namespace detail {

/** State information used during serialization to bitcode.
*/
class Serializer
{
public:
    Config const& config_;
    Reporter& R_;
    bool PublicOnly;
    int LineNumber;
    StringRef File;
    bool IsFileInRootDir;

    Serializer(
        int LineNumber_, StringRef File_,
        bool IsFileInRootDir_, Config const& config,
        Reporter& R)
        : LineNumber(LineNumber_)
        , File(File_)
        , IsFileInRootDir(IsFileInRootDir_)
        , config_(config)
        , R_(R)
        , PublicOnly(! config_.includePrivate())
    {
    }

    SerializeResult build(NamespaceDecl const* D);
    SerializeResult build(CXXRecordDecl const* D);
    SerializeResult build(CXXMethodDecl const* D);
    SerializeResult build(FunctionDecl  const* D);
    SerializeResult build(TypedefDecl   const* D);
    SerializeResult build(TypeAliasDecl const* D);
    SerializeResult build(EnumDecl      const* D);
};

} // detail

/** Return a serialized result for an AST Node.
*/
template<class Decl>
SerializeResult
serialize(
    Decl const* D, int LineNumber, StringRef File,
    bool IsFileInRootDir, Config const& config,
    Reporter& R)
{
    return detail::Serializer(LineNumber, File,
        IsFileInRootDir, config, R).build(D);
}

} // mrdox
} // clang

#endif

