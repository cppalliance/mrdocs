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

#include <mrdox/meta/TemplateParam.hpp>
#include <clang/AST/ASTContext.h>
#include <clang/AST/TemplateBase.h>
#include <clang/Lex/Lexer.h>

namespace clang {
namespace mrdox {

namespace {

std::string
getSourceCode(
    Decl const& D,
    SourceRange const& R)
{
    return Lexer::getSourceText(
        CharSourceRange::getTokenRange(R),
        D.getASTContext().getSourceManager(),
        D.getASTContext().getLangOpts()).str();
}

} // (anon)

TemplateParamInfo::
TemplateParamInfo(
    NamedDecl const& ND)
    : Contents(getSourceCode(ND, ND.getSourceRange()))
{
}

TemplateParamInfo::
TemplateParamInfo(
    clang::Decl const& D,
    TemplateArgument const& Arg)
{
    std::string Str;
    llvm::raw_string_ostream Stream(Str);
    Arg.print(PrintingPolicy(D.getLangOpts()), Stream, false);
    Contents = Str;
}

} // mrdox
} // clang
