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
#include "ParseJavadoc.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <mrdox/meta/Javadoc.hpp>
#include <clang/AST/AST.h>
#include <string>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

// VFALCO THIS SHOULD BE OBSOLETE
std::string serialize(Info const& I);

/** Holds a serialized declaration.
*/
struct SerializedDecl
{
    SymbolID id;
    std::string bitcode;

    bool empty() const noexcept
    {
        return bitcode.empty();
    }
};

/** Holds the result of serializing a Decl.

    This can result in two bitcodes. One for
    the declaration itself, and possibly one
    for the parent which is referenced by the
    decl.
*/
struct SerializeResult
{
    SerializedDecl first;
    SerializedDecl second;
};

class Serializer
{
public:
    Config const& config_;
    Reporter& R_;
    bool PublicOnly;
    int LineNumber;
    StringRef File;
    bool IsFileInRootDir;
    Javadoc jd_;

    Serializer(
        int LineNumber_,
        StringRef File_,
        bool IsFileInRootDir_,
        Config const& config,
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

template<class Decl>
SerializeResult
serialize(
    Decl const* D,
    int LineNumber,
    StringRef File,
    bool IsFileInRootDir,
    Config const& config,
    Reporter& R)
{
    Serializer sr(
        LineNumber,
        File,
        IsFileInRootDir,
        config,
        R);
    return sr.build(D);
}

} // mrdox
} // clang

#endif

