//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/AST/MacroCollector.hpp>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/Token.h>
#include <string>
#include <vector>

namespace mrdocs {

namespace {

bool
inSyntheticBuffer(
    clang::SourceLocation loc,
    clang::SourceManager const& SM)
{
    // `<built-in>` holds target predefines (`_MT`, `_MSC_VER`, ...);
    // `<command line>` holds `-D` macros from the command line
    // (e.g. mrdocs's own `__MRDOCS__`). Neither is user source.
    return SM.isInSystemHeader(loc) ||
           SM.isWrittenInBuiltinFile(loc) ||
           SM.isWrittenInCommandLineFile(loc);
}

bool
shouldSkip(
    clang::MacroInfo const* MI,
    clang::SourceLocation defLoc,
    clang::SourceManager const& SM)
{
    if (!MI || MI->isBuiltinMacro() || defLoc.isInvalid())
    {
        return true;
    }
    return inSyntheticBuffer(defLoc, SM);
}

std::vector<std::string>
gatherParameters(clang::MacroInfo const* MI)
{
    std::vector<std::string> result;
    // For variadic macros, Clang appends a synthetic
    // `__VA_ARGS__` identifier as the last entry of
    // `MI->params()`. Drop it: variadicness is reported
    // via `MacroDefinition::IsVariadic`, mirroring how
    // `FunctionSymbol::Params` excludes the trailing
    // C-style `...` and uses `IsVariadic` for the same
    // information.
    for (clang::IdentifierInfo const* P : MI->params())
    {
        if (P && P->getName() != "__VA_ARGS__")
        {
            result.emplace_back(P->getName().str());
        }
    }
    return result;
}

/* Strip whitespace between `#` and `define` from a directive's
   first line, and the same number of leading whitespace chars
   from each continuation line so that trailing backslashes stay
   column-aligned.

   A continuation line that starts with fewer whitespace chars
   than the count being stripped keeps whatever leading
   whitespace it has, so we never eat actual content.
*/
std::string
normalizeDefineDirective(std::string source)
{
    // Note: The caller passes the text of a `#define` directive, so
    // both finds succeed.
    std::size_t const hashPos = source.find('#');
    std::size_t const definePos = source.find("define", hashPos + 1);
    for (std::size_t i = hashPos + 1; i < definePos; ++i)
    {
        if (source[i] != ' ' && source[i] != '\t')
        {
            return source;
        }
    }
    std::size_t const stripCount = definePos - hashPos - 1;
    if (stripCount == 0)
    {
        return source;
    }
    source.erase(hashPos + 1, stripCount);
    std::size_t pos = source.find('\n');
    while (pos != std::string::npos)
    {
        std::size_t const lineStart = pos + 1;
        std::size_t available = 0;
        while (available < stripCount &&
               lineStart + available < source.size() &&
               (source[lineStart + available] == ' ' ||
                source[lineStart + available] == '\t'))
        {
            ++available;
        }
        if (available > 0)
        {
            source.erase(lineStart, available);
        }
        pos = source.find('\n', lineStart);
    }
    return source;
}

/* Verbatim source from the start of the line containing the
   macro definition through the end of the macro definition,
   with the normalization provided by `normalizeDefineDirective`.
*/
std::string
extractSource(
    clang::MacroInfo const* MI,
    clang::SourceManager const& SM,
    clang::LangOptions const& LO)
{
    clang::SourceLocation const defLoc = MI->getDefinitionLoc();
    clang::FileID const fileId = SM.getFileID(defLoc);
    unsigned const line = SM.getSpellingLineNumber(defLoc);
    clang::SourceLocation const lineStart =
        SM.translateLineCol(fileId, line, 1);
    // `getDefinitionEndLoc()` returns the start of the last
    // token (not past it), so `getTokenRange` is required to
    // include it in the extracted text.
    std::string const raw = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(
            lineStart, MI->getDefinitionEndLoc()),
        SM, LO).str();
    return normalizeDefineDirective(raw);
}

MacroDefinition
buildDefinition(
    clang::MacroInfo const* MI,
    clang::IdentifierInfo const* II,
    clang::SourceLocation defLoc,
    clang::SourceManager const& SM,
    clang::LangOptions const& LO)
{
    MacroDefinition m;
    m.Name = II->getName().str();
    m.DefLoc = defLoc;
    m.ClangMacro = MI;
    m.IsObjectLike = MI->isObjectLike();
    m.IsVariadic = MI->isVariadic();
    m.Parameters = gatherParameters(MI);
    m.Source = extractSource(MI, SM, LO);
    return m;
}

} // unnamed namespace

void
MacroCollector::
MacroDefined(
    clang::Token const& MacroNameTok,
    clang::MacroDirective const* MD)
{
    if (!MD)
    {
        return;
    }
    clang::MacroInfo const* const MI = MD->getMacroInfo();
    clang::SourceLocation const defLoc = MI ?
        MI->getDefinitionLoc() : clang::SourceLocation();
    clang::SourceManager const& SM = pp_.getSourceManager();
    clang::IdentifierInfo const* const II = MacroNameTok.getIdentifierInfo();
    if (!II || shouldSkip(MI, defLoc, SM))
    {
        return;
    }
    sink_.push_back(buildDefinition(MI, II, defLoc, SM, pp_.getLangOpts()));
}

} // mrdocs
