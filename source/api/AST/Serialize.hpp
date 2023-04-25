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

#ifndef MRDOX_API_AST_SERIALIZE_HPP
#define MRDOX_API_AST_SERIALIZE_HPP

#include <mrdox/Platform.hpp>
#include "clangASTComment.hpp"
#include "Bitcode.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/DeclFriend.h>
#include <clang/AST/Mangle.h>
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
struct SerializeResult
{
    llvm::SmallVector<Bitcode, 3> bitcodes;

    template<std::same_as<Bitcode>... Args>
    SerializeResult(
        Args&&... args)
    {
        (bitcodes.emplace_back(std::forward<Args>(args)), ...);
    }
};

/** State information used during serialization to bitcode.
*/
class Serializer
{
public:
    MangleContext& mc;
    Config const& config_;
    Reporter& R_;
    StringRef File;
    int LineNumber;
    bool PublicOnly;
    bool IsFileInRootDir;

    Serializer(
        MangleContext& mc_,
        int LineNumber_,
        StringRef File_,
        bool IsFileInRootDir_,
        Config const& config,
        Reporter& R)
        : mc(mc_)
        , config_(config)
        , R_(R)
        , File(File_)
        , LineNumber(LineNumber_)
        , PublicOnly(! config_.includePrivate())
        , IsFileInRootDir(IsFileInRootDir_)
    {
    }

    SerializeResult build(NamespaceDecl   const* D);
    SerializeResult build(CXXRecordDecl   const* D);
    SerializeResult build(CXXMethodDecl   const* D);
    SerializeResult build(FriendDecl      const* D);
    SerializeResult build(UsingDecl       const* D);
    SerializeResult build(UsingShadowDecl const* D);
    SerializeResult build(FunctionDecl    const* D);
    SerializeResult build(TypedefDecl     const* D);
    SerializeResult build(TypeAliasDecl   const* D);
    SerializeResult build(EnumDecl        const* D);
};

} // mrdox
} // clang

#endif

