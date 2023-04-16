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

//
// This file implements the serializing functions fro the clang-doc tool. Given
// a particular declaration, it collects the appropriate information and returns
// a serialized bitcode string for the declaration.
//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_SERIALIZE_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_SERIALIZE_H

#include "clangASTComment.hpp"
#include "ParseJavadoc.hpp"
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <mrdox/meta/Javadoc.hpp>
#include <clang/AST/AST.h>
#include <string>
#include <vector>

namespace clang {
namespace mrdox {

// The first element will contain the relevant information about the declaration
// passed as parameter.
// The second element will contain the relevant information about the
// declaration's parent, it can be a NamespaceInfo or RecordInfo.
// Both elements can be nullptrs if the declaration shouldn't be handled.
// When the declaration is handled, the first element will be a nullptr for
// EnumDecl, FunctionDecl and CXXMethodDecl; they are only returned wrapped in
// its parent scope. For NamespaceDecl and RecordDecl both elements are not
// nullptr.
std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
buildInfo(NamespaceDecl const* D, Javadoc jd, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly, Reporter& R);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
buildInfo(RecordDecl const* D, Javadoc jd, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly, Reporter& R);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
buildInfo(EnumDecl const* D, Javadoc jd, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly, Reporter& R);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
buildInfo(FunctionDecl const* D, Javadoc jd, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly, Reporter& R);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
buildInfo(CXXMethodDecl const* D, Javadoc jd, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly, Reporter& R);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
buildInfo(TypedefDecl const* D, Javadoc jd, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly, Reporter& R);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
buildInfo(TypeAliasDecl const* D, Javadoc jd, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly, Reporter& R);

template<class Decl, class... Args>
std::pair<
    std::unique_ptr<Info>,
    std::unique_ptr<Info>>
buildInfoPair(
    Decl const* D,
    Args&&... args)
{
    Javadoc jd;

    // TODO investigate whether we can use
    // ASTContext::getCommentForDecl instead of
    // this logic. See also similar code in Mapper.cpp.
    RawComment* RC = D->getASTContext().getRawCommentForDeclNoCache(D);
    if(RC)
    {
        RC->setAttached();
        jd = parseJavadoc(RC, D->getASTContext(), D);
    }

    return buildInfo(D, std::move(jd), std::forward<Args>(args)...);
}

// Function to hash a given USR value for storage.
// As USRs (Unified Symbol Resolution) could be large, especially for functions
// with long type arguments, we use 160-bits SHA1(USR) values to
// guarantee the uniqueness of symbols while using a relatively small amount of
// memory (vs storing USRs directly).
SymbolID hashUSR(llvm::StringRef USR);

std::string serialize(Info const& I);

} // mrdox
} // clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_SERIALIZE_H
