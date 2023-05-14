//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "FunctionKind.hpp"
#include "api/Support/TypeTraits.hpp"
#include "api/Support/Debug.hpp"
#include <clang/Basic/OperatorKinds.h>
#include <utility>

namespace clang {
namespace mrdox {

static_assert(
    to_underlying(FunctionKind::NUM_OVERLOADED_OPERATORS) ==
    to_underlying(OverloadedOperatorKind::NUM_OVERLOADED_OPERATORS));

llvm::StringRef
getFunctionKindString(
    FunctionKind kind)
{
    struct Item
    {
        char const* token;
        char const* name;
        FunctionKind kind;
    };
    using FK = FunctionKind;

    // short operator names from:
    // http://www.int0x80.gr/papers/name_mangling.pdf
    static constinit Item const tab[] = {
        /*
            ::= ps # + (unary)
            ::= ng # - (unary)
            ::= ad # & (unary)
            ::= de # * (unary) 
        */
        { "",          "",   FK::Plain },
        { "new",       "nw", FK::OpNew },
        { "delete",    "dl", FK::OpDelete },
        { "new[]",     "na", FK::OpArray_New },
        { "delete[]",  "da", FK::OpArray_Delete },
        { "+",         "pl", FK::OpPlus },
        { "-",         "mi", FK::OpMinus },
        { "*",         "ml", FK::OpStar },
        { "/",         "dv", FK::OpSlash },
        { "%",         "rm", FK::OpPercent },
        { "^",         "xor",FK::OpCaret },
        { "&",         "an", FK::OpAmp },
        { "|",         "or", FK::OpPipe },
        { "~",         "co", FK::OpTilde },
        { "!",         "nt", FK::OpExclaim },
        { "=",         "aS", FK::OpEqual },
        { "<",         "lt", FK::OpLess },
        { ">",         "gt", FK::OpGreater },
        { "+=",        "pL", FK::OpPlusEqual },
        { "-=",        "mI", FK::OpMinusEqual },
        { "*=",        "mL", FK::OpStarEqual },
        { "/=",        "dV", FK::OpSlashEqual },
        { "%=",        "rM", FK::OpPercentEqual },
        { "^=",        "eO", FK::OpCaretEqual },
        { "&=",        "aN", FK::OpAmpEqual },
        { "|=",        "oR", FK::OpPipeEqual },
        { "<<",        "ls", FK::OpLessLess },
        { ">>",        "rs", FK::OpGreaterGreater },
        { "<<=",       "lS", FK::OpLessLessEqual },
        { ">>=",       "rS", FK::OpGreaterGreaterEqual },
        { "==",        "eq", FK::OpEqualEqual },
        { "!=",        "ne", FK::OpExclaimEqual },
        { "<=",        "le", FK::OpLessEqual },
        { ">=",        "ge", FK::OpGreaterEqual },
        { "<=>",       "ss", FK::OpSpaceship },
        { "&&",        "aa", FK::OpAmpAmp },
        { "||",        "oo", FK::OpPipePipe },
        { "++",        "pp", FK::OpPlusPlus },
        { "--",        "mm", FK::OpMinusMinus },
        { ",",         "cm", FK::OpComma },
        { "->*",       "pm", FK::OpArrowStar },
        { "->",        "pt", FK::OpArrow },
        { "()",        "cl", FK::OpCall },
        { "[]",        "ix", FK::OpSubscript },
        { "?",         "qu", FK::OpConditional },
        { "co_await",  "ca", FK::OpCoawait },
        { "~",         "dt", FK::Destructor },
        { "",          "ct", FK::Constructor },
        { "",          "cv", FK::Conversion }
    };
    Assert(tab[to_underlying(kind)].kind == kind);
    return tab[to_underlying(kind)].name;
}

} // mrdox
} // clang

