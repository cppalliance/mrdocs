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

#include "Mapper.h"
#include "BitcodeWriter.h"
#include "Serialize.h"
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
MapASTVisitor::
HandleTranslationUnit(
    ASTContext& Context)
{
    TraverseDecl(Context.getTranslationUnitDecl());
}

template<typename T>
bool
MapASTVisitor::
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
        getFile(D, D->getASTContext(), cfg.SourceRoot, IsFileInRootDir);
    auto I = serialize::emitInfo(D, getComment(D, D->getASTContext()),
        getLine(D, D->getASTContext()), File,
        IsFileInRootDir, cfg.PublicOnly);

    // A null in place of I indicates that the serializer is skipping this decl
    // for some reason (e.g. we're only reporting public decls).
    if (I.first)
        cfg.ECtx->reportResult(llvm::toHex(llvm::toStringRef(I.first->USR)),
            serialize::serialize(I.first));
    if (I.second)
        cfg.ECtx->reportResult(llvm::toHex(llvm::toStringRef(I.second->USR)),
            serialize::serialize(I.second));
    return true;
}

bool
MapASTVisitor::
VisitNamespaceDecl(
    NamespaceDecl const* D)
{
    return mapDecl(D);
}

bool
MapASTVisitor::
VisitRecordDecl(
    RecordDecl const* D)
{
    return mapDecl(D);
}

bool
MapASTVisitor::
VisitEnumDecl(
    EnumDecl const* D)
{
    return mapDecl(D);
}

bool
MapASTVisitor::
VisitCXXMethodDecl(
    CXXMethodDecl const* D)
{
    return mapDecl(D);
}

bool
MapASTVisitor::
VisitFunctionDecl(
    FunctionDecl const* D)
{
    // Don't visit CXXMethodDecls twice
    if (isa<CXXMethodDecl>(D))
        return true;
    return mapDecl(D);
}

bool
MapASTVisitor::
VisitTypedefDecl(TypedefDecl const* D)
{
    return mapDecl(D);
}

bool
MapASTVisitor::
VisitTypeAliasDecl(TypeAliasDecl const* D)
{
    return mapDecl(D);
}

comments::FullComment*
MapASTVisitor::
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
MapASTVisitor::
getLine(
    NamedDecl const* D,
    ASTContext const& Context) const
{
    return Context.getSourceManager().getPresumedLoc(
        D->getBeginLoc()).getLine();
}

llvm::SmallString<128>
MapASTVisitor::
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

} // namespace mrdox
} // namespace clang
