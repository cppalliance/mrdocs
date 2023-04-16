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

#include "Commands.hpp"
#include "utility.hpp"
#include "ast/BitcodeWriter.hpp"
#include "ast/Serialize.hpp"
#include "ast/FrontendAction.hpp"
#include <mrdox/Corpus.hpp>
#include <clang/Index/USRGeneration.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>

#if 0
#include <mrdox/MetadataFwd.hpp>
#include <clang/AST/ASTConsumer.h>
#include <utility>
#include <unordered_map>
#endif

//
// This file implements the Mapper piece of the clang-doc tool. It implements
// a RecursiveASTVisitor to look at each declaration and populate the info
// into the internal representation. Each seen declaration is serialized to
// to bitcode and written out to the ExecutionContext as a KV pair where the
// key is the declaration's USR and the value is the serialized bitcode.
//

namespace clang {
namespace mrdox {

/** Visits AST and converts it to our metadata.
*/
class Visitor
    : public RecursiveASTVisitor<Visitor>
    , public ASTConsumer
{
    struct FileFilter
    {
        llvm::SmallString<0> prefix;
        bool include = true;
    };

    tooling::ExecutionContext& exc_;
    Config const& config_;
    Reporter& R_;
    std::unordered_map<
        clang::SourceLocation::UIntTy,
        FileFilter> fileFilter_;

public:
    Visitor(
        tooling::ExecutionContext& exc,
        Config const& config,
        Reporter& R) noexcept
        : exc_(exc)
        , config_(config)
        , R_(R)
    {
    }

//private:
    void HandleTranslationUnit(ASTContext& Context) override;
    bool VisitNamespaceDecl(NamespaceDecl const* D);
    bool VisitRecordDecl(RecordDecl const* D);
    bool VisitEnumDecl(EnumDecl const* D);
    bool VisitCXXMethodDecl(CXXMethodDecl const* D);
    bool VisitFunctionDecl(FunctionDecl const* D);
    bool VisitTypedefDecl(TypedefDecl const* D);
    bool VisitTypeAliasDecl(TypeAliasDecl const* D);

private:
    template <typename T>
    bool mapDecl(T const* D);

    int
    getLine(
        NamedDecl const* D,
        ASTContext const& Context) const;

    llvm::SmallString<128>
    getFile(
        NamedDecl const* D, 
        ASTContext const& Context,
        StringRef RootDir,
        bool& IsFileInRootDir) const;

    comments::FullComment*
    getComment(
        NamedDecl const* D,
        ASTContext const& Context) const;
};

//------------------------------------------------

/*  An instance of Visitor runs on one translation unit.
*/
void
Visitor::
HandleTranslationUnit(
    ASTContext& Context)
{
    initCustomCommentCommands(Context);

    llvm::Optional<llvm::StringRef> filePath = 
        Context.getSourceManager().getNonBuiltinFilenameForID(
            Context.getSourceManager().getMainFileID());
    if(filePath)
    {
        llvm::SmallString<0> s(*filePath);
        convert_to_slash(s);
        if(config_.shouldVisitTU(s))
            TraverseDecl(Context.getTranslationUnitDecl());
    }
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

    auto I = buildInfoPair(
        D,
        getComment(D, D->getASTContext()),
        getLine(D, D->getASTContext()),
        filePath,
        IsFileInRootDir,
        ! config_.includePrivate(),
        R_);

    // A null in place of I indicates that the
    // serializer is skipping this decl for some
    // reason (e.g. we're only reporting public decls).
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

//------------------------------------------------

namespace {

struct Action
    : public clang::ASTFrontendAction
{
    Action(
        tooling::ExecutionContext& exc,
        Config const& config,
        Reporter& R) noexcept
        : exc_(exc)
        , config_(config)
        , R_(R)
    {
    }

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(
        clang::CompilerInstance& Compiler,
        llvm::StringRef InFile) override
    {
        return std::make_unique<Visitor>(exc_, config_, R_);
    }

private:
    tooling::ExecutionContext& exc_;
    Config const& config_;
    Reporter& R_;
};

struct Factory : tooling::FrontendActionFactory
{
    Factory(
        tooling::ExecutionContext& exc,
        Config const& config,
        Reporter& R) noexcept
        : exc_(exc)
        , config_(config)
        , R_(R)
    {
    }

    std::unique_ptr<FrontendAction>
    create() override
    {
        return std::make_unique<Action>(exc_, config_, R_);
    }

private:
    tooling::ExecutionContext& exc_;
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
