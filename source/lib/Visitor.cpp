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
#include <llvm/Support/Path.h>

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
    namespace path = llvm::sys::path;

    clang::SourceManager const& sm =
        D->getASTContext().getSourceManager();

    if(sm.isInSystemHeader(D->getLocation()))
    {
        // skip system header
        return true;
    }

    if(D->getParentFunctionOrMethod())
    {
        // skip function-local declarations
        return true;
    }

    llvm::SmallString<512> filePath;
    clang::PresumedLoc const loc =
        sm.getPresumedLoc(D->getBeginLoc());
    auto result = fileFilter_.emplace(
        loc.getIncludeLoc().getRawEncoding(),
        FileFilter());
    if(result.second)
    {
        // new element
        filePath = loc.getFilename();
        FileFilter& ff = result.first->second;
        ff.exclude = config_.filterSourceFile(filePath, ff.prefix);
        if(ff.exclude)
            return true;
        path::replace_path_prefix(filePath, ff.prefix, "");
    }
    else
    {
        // existing element
        FileFilter const& ff = result.first->second;
        if(ff.exclude)
            return true;
        filePath = loc.getFilename();
        path::replace_path_prefix(filePath, ff.prefix, "");
    }

    // If there is an error generating a USR for the decl, skip this decl.
    {
        llvm::SmallString<128> USR;
        if (index::generateUSRForDecl(D, USR))
        {
            // VFALCO report this, it seems to never happen
            return true;
        }
    }

    // VFALCO is this right?
    bool const IsFileInRootDir = true;
    auto I = emitInfo(
        D,
        getComment(D, D->getASTContext()),
        getLine(D, D->getASTContext()),
        filePath,
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

} // mrdox
} // clang
