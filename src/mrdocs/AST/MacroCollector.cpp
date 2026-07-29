//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "MacroCollector.hpp"
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/Token.h>

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
    // Record only the raw Clang handles; the visitor derives the
    // `MacroSymbol` from them in `populate`, like any other symbol.
    sink_.push_back(CollectedMacro{II, MI});
}

} // mrdocs
