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

#include "Representation.h"
#include "clang/AST/AST.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5054) // C5054: operator '+': deprecated between enumerations of different types
#include "clang/AST/CommentVisitor.h"
#pragma warning(pop)
#endif
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
emitInfo(const NamespaceDecl *D, const comments::FullComment *FC, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
emitInfo(const RecordDecl *D, const comments::FullComment *FC, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
emitInfo(const EnumDecl *D, const comments::FullComment *FC, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
emitInfo(const FunctionDecl *D, const comments::FullComment *FC, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
emitInfo(const CXXMethodDecl *D, const comments::FullComment *FC, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
emitInfo(const TypedefDecl *D, const comments::FullComment *FC, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly);

std::pair<std::unique_ptr<Info>, std::unique_ptr<Info>>
emitInfo(const TypeAliasDecl *D, const comments::FullComment *FC, int LineNumber,
         StringRef File, bool IsFileInRootDir, bool PublicOnly);

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
