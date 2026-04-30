//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_MACRO_HPP
#define MRDOCS_API_METADATA_SYMBOL_MACRO_HPP

#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>
#include <vector>

namespace mrdocs {

/** Info for preprocessor macros.

    Covers both object-like and function-like macros.
*/
struct MacroSymbol final
    : SymbolCommonBase<SymbolKind::Macro>
{
    /** Whether this is a function-like macro.
    */
    bool IsFunctionLike = false;

    /** The names of the macro's parameters.

        Empty for object-like macros. For variadic
        function-like macros, this lists only the
        named parameters; variadicness is indicated
        by @ref IsVariadic.
    */
    std::vector<std::string> Parameters;

    /** Whether the macro takes a variadic argument list.

        True for macros declared with `...`, such as
        `#define LOG(fmt, ...) ...`.
    */
    bool IsVariadic = false;

    /** The full source of the macro definition.

        Captured from the start of the line containing
        the `#define` directive through the end of the
        macro definition, line continuations and all,
        with one normalization: whitespace between `#`
        and `define` (typically used when the macro
        definition is in a `#if`/`#ifdef`/`#ifndef`),
        and the matching leading whitespace on
        continuation lines, is stripped; so the
        definition in the synopsis reads as a
        top-level directive without a surrounding
        `#if`/`#ifdef`/`#ifndef`.
    */
    std::string Source;

    //--------------------------------------------

    /** Create a macro symbol bound to an ID.
    */
    explicit MacroSymbol(SymbolID const& ID) noexcept
        : SymbolCommonBase(ID)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    MacroSymbol,
    (SymbolCommonBase<SymbolKind::Macro>),
    (IsFunctionLike, Parameters, IsVariadic, Source)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_MACRO_HPP
