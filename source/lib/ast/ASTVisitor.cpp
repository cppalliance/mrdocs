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

#include "ASTVisitor.hpp"
#include "Commands.hpp"
#include "utility.hpp"
#include "Serialize.hpp"
#include <clang/Index/USRGeneration.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

// An instance of Visitor runs on one translation unit.
void
ASTVisitor::
HandleTranslationUnit(
    ASTContext& Context)
{
    initCustomCommentCommands(Context);

    llvm::Optional<llvm::StringRef> filePath = 
        Context.getSourceManager().getNonBuiltinFilenameForID(
            Context.getSourceManager().getMainFileID());
    if(! filePath)
        return;

    llvm::SmallString<0> s(*filePath);
    convert_to_slash(s);
    if(! config_.shouldVisitTU(s))
        return;

    //mc_.reset(MicrosoftMangleContext::create(
    //mc_.reset(ItaniumMangleContext::create(
        //Context, Context.getDiagnostics()));

    TraverseDecl(Context.getTranslationUnitDecl());
}

template<typename T>
bool
ASTVisitor::
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
    if(! result.second)
    {
        // cached filter entry already exists
        FileFilter const& ff = result.first->second;
        if(! ff.include)
            return true;
        filePath = loc.getFilename(); // native
        convert_to_slash(filePath);
        // VFALCO we could assert that the prefix
        //        matches and just lop off the
        //        first ff.prefix.size() characters.
        path::replace_path_prefix(filePath, ff.prefix, "");
    }
    else
    {
        // new element
        filePath = loc.getFilename();
        convert_to_slash(filePath);
        FileFilter& ff = result.first->second;
        ff.include = config_.shouldVisitFile(filePath, ff.prefix);
        if(! ff.include)
            return true;
        // VFALCO we could assert that the prefix
        //        matches and just lop off the
        //        first ff.prefix.size() characters.
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

    auto res = serialize(
        D,
        *mc_,
        getLine(D, D->getASTContext()),
        filePath,
        IsFileInRootDir,
        config_,
        R_);

    // An empty serialized declaration means that
    // the serializer is skipping this declaration
    // for some reason. For example it is private,
    // or the declaration is enclosed in the parent.
    if(! res.first.empty())
        insertBitcode(ex_, std::move(res.first));
    if(! res.second.empty())
        insertBitcode(ex_, std::move(res.second));
    return true;
}

bool
ASTVisitor::
VisitNamespaceDecl(
    NamespaceDecl const* D)
{
    return mapDecl(D);
}

bool
ASTVisitor::
VisitCXXRecordDecl(
    CXXRecordDecl const* D)
{
    return mapDecl(D);
}

bool
ASTVisitor::
VisitRecordDecl(
    RecordDecl const* D)
{
    // Don't visit CXXRecordDecl twice
    if (isa<CXXRecordDecl>(D))
        return true;
    //return mapDecl(D);
    llvm_unreachable("C record in C++?");
}

bool
ASTVisitor::
VisitEnumDecl(
    EnumDecl const* D)
{
    return mapDecl(D);
}

bool
ASTVisitor::
VisitCXXDestructorDecl(
    CXXDestructorDecl const* D)
{
    return mapDecl(D);
}

bool
ASTVisitor::
VisitCXXConstructorDecl(
    CXXConstructorDecl const* D)
{
    return mapDecl(D);
}

bool
ASTVisitor::
VisitCXXMethodDecl(
    CXXMethodDecl const* D)
{
    // Don't visit twice
    if( isa<CXXDestructorDecl>(D) ||
        isa<CXXConstructorDecl>(D))
        return true;
    return mapDecl(D);
}

bool
ASTVisitor::
VisitFriendDecl(
    FriendDecl const* D)
{
    return true;
}

bool
ASTVisitor::
VisitFunctionDecl(
    FunctionDecl const* D)
{
    // Don't visit CXXMethodDecls twice
    if (isa<CXXMethodDecl>(D))
        return true;
    return mapDecl(D);
}

bool
ASTVisitor::
VisitTypedefDecl(TypedefDecl const* D)
{
    return mapDecl(D);
}

bool
ASTVisitor::
VisitTypeAliasDecl(TypeAliasDecl const* D)
{
    return mapDecl(D);
}

int
ASTVisitor::
getLine(
    NamedDecl const* D,
    ASTContext const& Context) const
{
    return Context.getSourceManager().getPresumedLoc(
        D->getBeginLoc()).getLine();
}

//------------------------------------------------

namespace {

struct Action
    : public clang::ASTFrontendAction
{
    Action(
        tooling::ExecutionContext& exc,
        Config const& config,
        Reporter& R) noexcept
        : ex_(exc)
        , config_(config)
        , R_(R)
    {
    }

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(
        clang::CompilerInstance& Compiler,
        llvm::StringRef InFile) override
    {
        return std::make_unique<ASTVisitor>(ex_, config_, R_);
    }

private:
    tooling::ExecutionContext& ex_;
    Config const& config_;
    Reporter& R_;
};

struct Factory : tooling::FrontendActionFactory
{
    Factory(
        tooling::ExecutionContext& exc,
        Config const& config,
        Reporter& R) noexcept
        : ex_(exc)
        , config_(config)
        , R_(R)
    {
    }

    std::unique_ptr<FrontendAction>
    create() override
    {
        return std::make_unique<Action>(ex_, config_, R_);
    }

private:
    tooling::ExecutionContext& ex_;
    Config const& config_;
    Reporter& R_;
};

} // (anon)

std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    tooling::ExecutionContext& exc,
    Config const& config,
    Reporter& R)
{
    return std::make_unique<Factory>(exc, config, R);
}

} // mrdox
} // clang
