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
#include "Serialize.hpp"
#include "Support/Path.hpp"
#include <mrdox/Debug.hpp>
#include <clang/Index/USRGeneration.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <llvm/ADT/Optional.h>
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

// Returns `true` if something got mapped
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
        return false;
    }

    if(D->getParentFunctionOrMethod())
    {
        // skip function-local declarations
        return false;
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
            return false;
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
            return false;
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
            return false;
        }
    }

    // VFALCO is this right?
    bool const IsFileInRootDir = true;

    SerializeResult res;
    if constexpr(std::is_same_v<FriendDecl, T>)
    {
        res = Serializer(
            *mc_,
            0,//getLine(D, D->getASTContext()),
            filePath,
            IsFileInRootDir,
            config_,
            R_).build(D);
    }
    else
    {
        res = Serializer(
            *mc_,
            getLine(D, D->getASTContext()),
            filePath,
            IsFileInRootDir,
            config_,
            R_).build(D);
    }

    if(res.bitcodes.empty())
        return false;

    for(auto&& bitcode : res.bitcodes)
        insertBitcode(ex_, std::move(bitcode));
    return true;
}

//------------------------------------------------

// Returning false from any of these
// functions will abort the _entire_ traversal

bool
ASTVisitor::
WalkUpFromNamespaceDecl(
    NamespaceDecl* D)
{
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXRecordDecl(
    CXXRecordDecl* D)
{
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXDestructorDecl(
    CXXDestructorDecl* D)
{
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXConstructorDecl(
    CXXConstructorDecl* D)
{
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXMethodDecl(
    CXXMethodDecl* D)
{
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromFriendDecl(
    FriendDecl* D)
{
    mapDecl(D);
    return true;
}

#if 0
bool
ASTVisitor::
WalkUpFromUsingDecl(
    UsingDecl* D)
{
    mapDecl(D);
    return true;
}
#endif

bool
ASTVisitor::
WalkUpFromUsingShadowDecl(
    UsingShadowDecl* D)
{
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromFunctionDecl(
    FunctionDecl* D)
{
    Assert(! dyn_cast<CXXMethodDecl>(D));
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromTypeAliasDecl(
    TypeAliasDecl* D)
{
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromTypedefDecl(
    TypedefDecl* D)
{
    mapDecl(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromEnumDecl(
    EnumDecl* D)
{
    mapDecl(D);
    return true;
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
