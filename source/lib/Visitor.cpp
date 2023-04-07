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

#include "BitcodeWriter.h"
#include "Error.hpp"
#include "Serialize.h"
#include <mrdox/Visitor.hpp>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5054) // C5054: operator '+': deprecated between enumerations of different types
#include <clang/AST/Comment.h>
#pragma warning(pop)
#endif
#include "clang/Index/USRGeneration.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Error.h"

namespace clang {
namespace mrdox {

void
Visitor::
HandleTranslationUnit(
    ASTContext& Context)
{
    TraverseDecl(Context.getTranslationUnitDecl());
}

void
Visitor::
reportResult(
    StringRef Key, StringRef Value)
{
    corpus_.toolResults->addResult(Key, Value);
}

template<typename T>
bool
Visitor::
mapDecl(T const* D)
{
    // If we're looking a decl not in user files, skip this decl.
    if (D->getASTContext().getSourceManager().isInSystemHeader(D->getLocation()))
        return true;

    // Skip function-internal decls.
    if (D->getParentFunctionOrMethod())
        return true;

    llvm::SmallString<128> USR;
    // If there is an error generating a USR for the decl, skip this decl.
    if (index::generateUSRForDecl(D, USR))
        return true;
    bool IsFileInRootDir;
    llvm::SmallString<128> File =
        getFile(D, D->getASTContext(), cfg_.SourceRoot, IsFileInRootDir);
    auto I = serialize::emitInfo(
        D,
        getComment(D, D->getASTContext()),
        getLine(D, D->getASTContext()),
        File,
        IsFileInRootDir,
        cfg_.PublicOnly);

    // A null in place of I indicates that the serializer is skipping this decl
    // for some reason (e.g. we're only reporting public decls).
    if (I.first)
        reportResult(
            llvm::toHex(llvm::toStringRef(I.first->USR)),
            serialize::serialize(I.first));
    if (I.second)
        reportResult(
            llvm::toHex(llvm::toStringRef(I.second->USR)),
            serialize::serialize(I.second));

    return true;
}

bool
Visitor::
VisitNamespaceDecl(
    NamespaceDecl const* D)
{
    return mapDecl(D);
}

bool
Visitor::
VisitRecordDecl(
    RecordDecl const* D)
{
    return mapDecl(D);
}

bool
Visitor::
VisitEnumDecl(
    EnumDecl const* D)
{
    return mapDecl(D);
}

bool
Visitor::
VisitCXXMethodDecl(
    CXXMethodDecl const* D)
{
    return mapDecl(D);
}

bool
Visitor::
VisitFunctionDecl(
    FunctionDecl const* D)
{
    // Don't visit CXXMethodDecls twice
    if (isa<CXXMethodDecl>(D))
        return true;
    return mapDecl(D);
}

// https://github.com/llvm/llvm-project/blob/466d554dcab39c3d42fe0c5b588b795e0e4b9d0d/clang/include/clang/AST/Type.h#L1566

bool
Visitor::
VisitTypedefDecl(TypedefDecl const* D)
{
    return mapDecl(D);
}

bool
Visitor::
VisitTypeAliasDecl(TypeAliasDecl const* D)
{
    return mapDecl(D);
}

comments::FullComment*
Visitor::
getComment(
    NamedDecl const* D,
    ASTContext const& Context) const
{
    RawComment* Comment = Context.getRawCommentForDeclNoCache(D);
    // FIXME: Move setAttached to the initial comment parsing.
    if (Comment)
    {
        Comment->setAttached();
        return Comment->parse(Context, nullptr, D);
    }
    return nullptr;
}

int
Visitor::
getLine(
    NamedDecl const* D,
    ASTContext const& Context) const
{
    return Context.getSourceManager().getPresumedLoc(
        D->getBeginLoc()).getLine();
}

llvm::SmallString<128>
Visitor::
getFile(NamedDecl const* D,
    ASTContext const& Context,
    llvm::StringRef RootDir,
    bool& IsFileInRootDir) const
{
    llvm::SmallString<128> File(
        Context.getSourceManager()
            .getPresumedLoc(D->getBeginLoc())
            .getFilename());
    IsFileInRootDir = false;
    if (RootDir.empty() || !File.startswith(RootDir))
        return File;
    IsFileInRootDir = true;
    llvm::SmallString<128> Prefix(RootDir);
    // replace_path_prefix removes the exact prefix provided. The result of
    // calling that function on ("A/B/C.c", "A/B", "") would be "/C.c", which
    // starts with a / that is not needed. This is why we fix Prefix so it always
    // ends with a / and the result has the desired format.
    if (!llvm::sys::path::is_separator(Prefix.back()))
        Prefix += llvm::sys::path::get_separator();
    llvm::sys::path::replace_path_prefix(File, Prefix, "");
    return File;
}

} // mrdox
} // clang
