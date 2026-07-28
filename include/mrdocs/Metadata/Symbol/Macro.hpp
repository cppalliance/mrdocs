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

        This is not derivable from @ref Parameters and
        @ref IsVariadic: a function-like macro can have an
        empty parameter list. `#define F` is object-like,
        while `#define F()` is function-like with no
        parameters, and only this flag tells them apart.
    */
    bool IsFunctionLike = false;

    /** The names of the macro's parameters.

        Empty for object-like macros and for function-like
        macros with no parameters. For variadic
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
    (IsFunctionLike, Parameters, IsVariadic)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_MACRO_HPP
