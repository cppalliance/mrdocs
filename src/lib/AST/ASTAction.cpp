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

#include "lib/AST/ASTAction.hpp"
#include "lib/AST/ASTVisitorConsumer.hpp"
#include <clang/Frontend/CompilerInstance.h>
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
    CI.getLangOpts().RetainCommentsFromSystemHeaders = true;

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
