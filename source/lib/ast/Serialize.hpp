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
    Config const& config_;
    Reporter& R_;
    bool PublicOnly;
    int LineNumber;
    StringRef File;
    bool IsFileInRootDir;
    Javadoc jd_;

public:
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

    using value_type = std::pair<
        std::unique_ptr<Info>, std::unique_ptr<Info>>;

    // The first element will contain the relevant information about the declaration
    // passed as parameter.
    // The second element will contain the relevant information about the
    // declaration's parent, it can be a NamespaceInfo or RecordInfo.
    // Both elements can be nullptrs if the declaration shouldn't be handled.
    // When the declaration is handled, the first element will be a nullptr for
    // EnumDecl, FunctionDecl and CXXMethodDecl; they are only returned wrapped in
    // its parent scope. For NamespaceDecl and RecordDecl both elements are not
    // nullptr.

    value_type buildInfo(NamespaceDecl const* D);
    value_type buildInfo(CXXRecordDecl const* D);
    value_type buildInfo(EnumDecl      const* D);
    value_type buildInfo(FunctionDecl  const* D);
    value_type buildInfo(CXXMethodDecl const* D);
    value_type buildInfo(TypedefDecl   const* D);
    value_type buildInfo(TypeAliasDecl const* D);

    template<class Decl>
    value_type
    buildInfoPair(
        Decl const* D)
    {
        Javadoc jd;

        // TODO investigate whether we can use
        // ASTContext::getCommentForDecl instead of
        // this logic. See also similar code in Mapper.cpp.
        RawComment* RC = D->getASTContext().getRawCommentForDeclNoCache(D);
        if(RC)
        {
            RC->setAttached();
            jd_ = parseJavadoc(RC, D->getASTContext(), D);
        }

        return buildInfo(D);
    }

    SerializeResult build(NamespaceDecl const* D);
    SerializeResult build(CXXRecordDecl const* D);
    SerializeResult build(EnumDecl      const* D);
    SerializeResult build(FunctionDecl  const* D);
    SerializeResult build(CXXMethodDecl const* D);
    SerializeResult build(TypedefDecl   const* D);
    SerializeResult build(TypeAliasDecl const* D);
};

template<class Decl>
std::array<SerializedDecl, 2>
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

