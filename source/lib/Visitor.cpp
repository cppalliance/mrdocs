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
#include "Serialize.h"
#include <mrdox/Corpus.hpp>
#include <mrdox/Visitor.hpp>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5054) // C5054: operator '+': deprecated between enumerations of different types
#include <clang/AST/Comment.h>
#pragma warning(pop)
#endif
#include <clang/Index/USRGeneration.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Error.h>

namespace clang {
namespace mrdox {

void
Visitor::
HandleTranslationUnit(
    ASTContext& Context)
{
    assert(! handleTranslationUnit_);
    handleTranslationUnit_ = true;
    TraverseDecl(Context.getTranslationUnitDecl());
}

template<typename T>
bool
Visitor::
mapDecl(T const* D)
{
    clang::SourceManager const& sm =
        D->getASTContext().getSourceManager();
    clang::SourceLocation const loc = D->getLocation();

    if(sm.isInSystemHeader(loc))
    {
        // skip system header
        return true;
    }

    if (D->getParentFunctionOrMethod())
    {
        // skip function-local declarations
        return true;
    }

    llvm::SmallString<128> filePath;
    filePath = sm.getPresumedLoc(D->getBeginLoc()).getFilename();
    if(config_.shouldSkipFile(filePath))
        return true;

    // If there is an error generating a USR for the decl, skip this decl.
    {
        llvm::SmallString<128> USR;
        if (index::generateUSRForDecl(D, USR))
        {
            // VFALCO report this, it seems to never happen
            return true;
        }
    }

    bool IsFileInRootDir;
    llvm::SmallString<128> File =
        getFile(D, D->getASTContext(), config_.SourceRoot, IsFileInRootDir);
    auto I = emitInfo(
        D,
        getComment(D, D->getASTContext()),
        getLine(D, D->getASTContext()),
        File,
        IsFileInRootDir,
        config_.PublicOnly);

    // A null in place of I indicates that the serializer is skipping this decl
    // for some reason (e.g. we're only reporting public decls).
    if (I.first)
        Corpus::reportResult(exc_, *I.first);
    if (I.second)
        Corpus::reportResult(exc_, *I.second);

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
