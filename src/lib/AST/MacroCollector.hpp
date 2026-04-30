//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_MACROCOLLECTOR_HPP
#define MRDOCS_LIB_AST_MACROCOLLECTOR_HPP

#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/PPCallbacks.h>
#include <string>
#include <vector>

namespace clang {
class MacroDirective;
class MacroInfo;
class Preprocessor;
class Token;
}

namespace mrdocs {

/** A captured `#define` from the preprocessor.

    Filled in by @ref MacroCollector when the preprocessor
    sees a macro definition. The visitor consumes these
    at the end of the translation unit and turns them into
    @ref MacroSymbol instances.
*/
struct MacroDefinition {
    /** The macro identifier.
    */
    std::string Name;

    /** The location of the macro name in the `#define`.
    */
    clang::SourceLocation DefLoc;

    /** The preprocessor's record of this definition.
    */
    clang::MacroInfo const* ClangMacro = nullptr;

    /** True for object-like macros (no parameter list).
    */
    bool IsObjectLike = true;

    /** True when the macro takes a variadic argument list.
    */
    bool IsVariadic = false;

    /** Names of the named parameters, in declaration order.
    */
    std::vector<std::string> Parameters;

    /** Full source of the macro definition, line continuations
        and all. Used as the synopsis at render time.
    */
    std::string Source;
};

/** Capture `MacroDefined` events from the preprocessor.

    Skip definitions in system headers and built-in macros;
    the visitor applies any further filtering.
*/
class MacroCollector final : public clang::PPCallbacks {
    std::vector<MacroDefinition>& sink_;
    clang::Preprocessor const& pp_;

public:
    /** Construct a collector that pushes into the given sink.
    */
    MacroCollector(
        std::vector<MacroDefinition>& sink,
        clang::Preprocessor const& pp) noexcept
        : sink_(sink)
        , pp_(pp)
    {
    }

    /** Record a `#define` directive.
    */
    void
    MacroDefined(
        clang::Token const& MacroNameTok,
        clang::MacroDirective const* MD) override;
};

} // mrdocs

#endif // MRDOCS_LIB_AST_MACROCOLLECTOR_HPP
