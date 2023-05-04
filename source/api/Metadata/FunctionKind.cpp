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
    static constinit Item const tab[] = {
        { "new",       "new",        FK::Plain },
        { "delete",    "del",        FK::OpDelete },
        { "new[]",     "arr_new",    FK::OpArray_New },
        { "delete[]",  "arr_del",    FK::OpArray_Delete },
        { "+",         "plus",       FK::OpPlus },
        { "-",         "minus",      FK::OpMinus },
        { "*",         "star",       FK::OpStar },
        { "/",         "slash",      FK::OpSlash },
        { "%",         "mod",        FK::OpPercent },
        { "^",         "xor",        FK::OpCaret },
        { "&",         "bitand",     FK::OpAmp },
        { "|",         "bitor",      FK::OpPipe },
        { "~",         "bitnot",     FK::OpTilde },
        { "!",         "not",        FK::OpExclaim },
        { "=",         "assign",     FK::OpEqual },
        { "<",         "lt",         FK::OpLess },
        { ">",         "gt",         FK::OpGreater },
        { "+=",        "plus_eq",    FK::OpPlusEqual },
        { "-=",        "minus_eq",   FK::OpMinusEqual },
        { "*=",        "star_eq",    FK::OpStarEqual },
        { "/=",        "slash_eq",   FK::OpSlashEqual },
        { "%=",        "mod_eq",     FK::OpPercentEqual },
        { "^=",        "xor_eq",     FK::OpCaretEqual },
        { "&=",        "and_eq",     FK::OpAmpEqual },
        { "|=",        "or_eq",      FK::OpPipeEqual },
        { "<<",        "lt_lt",      FK::OpLessLess },
        { ">>",        "gt_gt",      FK::OpGreaterGreater },
        { "<<=",       "lt_lt_eq",   FK::OpLessLessEqual },
        { ">>=",       "gt_gt_eq",   FK::OpGreaterGreaterEqual },
        { "==",        "eq",         FK::OpEqualEqual },
        { "!=",        "not_eq",     FK::OpExclaimEqual },
        { "<=",        "le",         FK::OpLessEqual },
        { ">=",        "ge",         FK::OpGreaterEqual },
        { "<=>",       "3way",       FK::OpSpaceship },
        { "&&",        "and",        FK::OpAmpAmp },
        { "||",        "or",         FK::OpPipePipe },
        { "++",        "inc",        FK::OpPlusPlus },
        { "--",        "dec",        FK::OpMinusMinus },
        { ",",         "comma",      FK::OpComma },
        { "->*",       "ptrmem",     FK::OpArrowStar },
        { "->",        "ptr",        FK::OpArrow },
        { "()",        "call",       FK::OpCall },
        { "[]",        "subs",       FK::OpSubscript },
        { "?",         "ternary",    FK::OpConditional },
        { "co_await",  "coawait",    FK::OpCoawait },
        { "~",         "dtor",       FK::Destructor },
        { "",          "ctor",       FK::Constructor },
        { "",          "conv",       FK::Conversion }
    };
    Assert(tab[to_underlying(kind)].kind == kind);
    return tab[to_underlying(kind)].name;
}

} // mrdox
} // clang

