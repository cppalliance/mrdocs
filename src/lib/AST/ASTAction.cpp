//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/AST/ASTAction.hpp>
#include <lib/AST/ASTVisitorConsumer.hpp>
#include <lib/AST/MrDocsFileSystem.hpp>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Parse/ParseAST.h>

namespace clang {
namespace mrdocs {

void
ASTAction::
ExecuteAction()
{
    CompilerInstance& CI = getCompilerInstance();
    if (!CI.hasPreprocessor())
    {
        return;
    }

    // Ensure comments in system headers are retained.
    // We may want them if, e.g., a declaration was extracted
    // as a dependency
    auto &Lang = CI.getLangOpts();
    Lang.RetainCommentsFromSystemHeaders = true;
    bool const buildShims =
        !config_.settings()->missingIncludePrefixes.empty() ||
        !config_.settings()->missingIncludeShims.empty();
    if (buildShims && missingSink_)
    {
        // Install missing symbol sink
        missingSink_->setStartParsing();
        auto &DE = CI.getDiagnostics();
        std::unique_ptr<clang::DiagnosticConsumer> prev = DE.takeClient();
        DE.setClient(
            new CollectingDiagConsumer(*missingSink_, std::move(prev), DE),
            /*ShouldOwnClient*/ true);

        // Turn on AST recovery:
        // Enable Clang’s recovery flags so it still builds
        // decls/exprs with placeholder types when something is broken.
        Lang.RecoveryAST = 1; // keep building AST nodes on errors
        Lang.RecoveryASTType = 1; // synthesize placeholder types

        // Mark stubbed prefixes as “system” for quieter diagnostics
        for (auto &prefix : config_.settings()->missingIncludePrefixes)
        {
            MRDOCS_CHECK_OR_CONTINUE(!prefix.empty());
            if (prefix.back() == '/')
            {
                CI.getHeaderSearchOpts().AddSystemHeaderPrefix(prefix, /*IsSystemHeader=*/true);
            }
            else
            {
                std::string p = prefix;
                p += '/';
                CI.getHeaderSearchOpts().AddSystemHeaderPrefix(p, /*IsSystemHeader=*/true);
            }
        }
    }

    // Skip function bodies:
    // We don’t need bodies to enumerate symbols. This eliminates
    // a ton of dependent code and template instantiations.
    auto &FrontendOpts = CI.getFrontendOpts();
    FrontendOpts.SkipFunctionBodies = true;

    if (!CI.hasSema())
    {
        CI.createSema(getTranslationUnitKind(), nullptr);
    }

    ParseAST(
        CI.getSema(),
        false, // ShowStats
        true); // SkipFunctionBodies
}

std::unique_ptr<clang::ASTConsumer>
ASTAction::
CreateASTConsumer(
    clang::CompilerInstance& Compiler,
    llvm::StringRef)
{
    return std::make_unique<ASTVisitorConsumer>(
        config_, ex_, Compiler);
}

} // mrdocs
} // clang
