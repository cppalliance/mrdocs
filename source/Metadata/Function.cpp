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

#include "Support/TypeTraits.hpp"
#include <mrdox/Metadata/Function.hpp>
#include <clang/Basic/OperatorKinds.h>
#include <utility>

namespace clang {
namespace mrdox {

namespace {

static_assert(
    to_underlying(FunctionKind::NUM_OVERLOADED_OPERATORS) ==
    to_underlying(OverloadedOperatorKind::NUM_OVERLOADED_OPERATORS));

struct Item
{
    char const* token;
    char const* name;
    FunctionKind kind;
    OverloadedOperatorKind ook;
};

// short operator names from:
// http://www.int0x80.gr/papers/name_mangling.pdf
using FK = FunctionKind;
using OOK = OverloadedOperatorKind;
static constinit Item const Table[] = {
    /*
        ::= ps # + (unary)
        ::= ng # - (unary)
        ::= ad # & (unary)
        ::= de # * (unary)
    */
    { "",          "",    FK::Plain,                 OOK::OO_None },
    { "new",       "nw",  FK::OpNew,                 OOK::OO_New },
    { "delete",    "dl",  FK::OpDelete,              OOK::OO_Delete },
    { "new[]",     "na",  FK::OpArray_New,           OOK::OO_Array_New },
    { "delete[]",  "da",  FK::OpArray_Delete,        OOK::OO_Array_Delete },
    { "+",         "pl",  FK::OpPlus,                OOK::OO_Plus },
    { "-",         "mi",  FK::OpMinus,               OOK::OO_Minus },
    { "*",         "ml",  FK::OpStar,                OOK::OO_Star },
    { "/",         "dv",  FK::OpSlash,               OOK::OO_Slash },
    { "%",         "rm",  FK::OpPercent,             OOK::OO_Percent },
    { "^",         "eo",  FK::OpCaret,               OOK::OO_Caret },
    { "&",         "an",  FK::OpAmp,                 OOK::OO_Amp },
    { "|",         "or",  FK::OpPipe,                OOK::OO_Pipe },
    { "~",         "co",  FK::OpTilde,               OOK::OO_Tilde },
    { "!",         "nt",  FK::OpExclaim,             OOK::OO_Exclaim },
    { "=",         "as",  FK::OpEqual,               OOK::OO_Equal },
    { "<",         "lt",  FK::OpLess,                OOK::OO_Less },
    { ">",         "gt",  FK::OpGreater,             OOK::OO_Greater },
    { "+=",        "ple", FK::OpPlusEqual,           OOK::OO_PlusEqual },
    { "-=",        "mie", FK::OpMinusEqual,          OOK::OO_MinusEqual },
    { "*=",        "mle", FK::OpStarEqual,           OOK::OO_StarEqual },
    { "/=",        "dve", FK::OpSlashEqual,          OOK::OO_SlashEqual },
    { "%=",        "rme", FK::OpPercentEqual,        OOK::OO_PercentEqual },
    { "^=",        "eoe", FK::OpCaretEqual,          OOK::OO_CaretEqual },
    { "&=",        "ane", FK::OpAmpEqual,            OOK::OO_AmpEqual },
    { "|=",        "ore", FK::OpPipeEqual,           OOK::OO_PipeEqual },
    { "<<",        "ls",  FK::OpLessLess,            OOK::OO_LessLess },
    { ">>",        "rs",  FK::OpGreaterGreater,      OOK::OO_GreaterGreater },
    { "<<=",       "lse", FK::OpLessLessEqual,       OOK::OO_LessLessEqual },
    { ">>=",       "rse", FK::OpGreaterGreaterEqual, OOK::OO_GreaterGreaterEqual },
    { "==",        "eq",  FK::OpEqualEqual,          OOK::OO_EqualEqual },
    { "!=",        "ne",  FK::OpExclaimEqual,        OOK::OO_ExclaimEqual },
    { "<=",        "le",  FK::OpLessEqual,           OOK::OO_LessEqual },
    { ">=",        "ge",  FK::OpGreaterEqual,        OOK::OO_GreaterEqual },
    { "<=>",       "ss",  FK::OpSpaceship,           OOK::OO_Spaceship },
    { "&&",        "aa",  FK::OpAmpAmp,              OOK::OO_AmpAmp },
    { "||",        "oo",  FK::OpPipePipe,            OOK::OO_PipePipe },
    { "++",        "pp",  FK::OpPlusPlus,            OOK::OO_PlusPlus },
    { "--",        "mm",  FK::OpMinusMinus,          OOK::OO_MinusMinus },
    { ",",         "cm",  FK::OpComma,               OOK::OO_Comma },
    { "->*",       "pm",  FK::OpArrowStar,           OOK::OO_ArrowStar },
    { "->",        "pt",  FK::OpArrow,               OOK::OO_Arrow },
    { "()",        "cl",  FK::OpCall,                OOK::OO_Call },
    { "[]",        "ix",  FK::OpSubscript,           OOK::OO_Subscript },
    { "?",         "qu",  FK::OpConditional,         OOK::OO_Conditional },
    { "co_await",  "ca",  FK::OpCoawait,             OOK::OO_Coawait },
    { "~",         "dt",  FK::Destructor,            OOK::OO_None },
    { "",          "ct",  FK::Constructor,           OOK::OO_None },
    { "",          "cv",  FK::Conversion,            OOK::OO_None }
};

} // (anon)

FunctionKind
getFunctionKind(
    OverloadedOperatorKind OOK) noexcept
{
    MRDOX_ASSERT(OOK < OverloadedOperatorKind::NUM_OVERLOADED_OPERATORS);
    MRDOX_ASSERT(Table[to_underlying(OOK)].ook == OOK);
    return Table[to_underlying(OOK)].kind;
}

std::string_view
getFunctionKindString(
    FunctionKind kind) noexcept
{
    MRDOX_ASSERT(Table[to_underlying(kind)].kind == kind);
    return Table[to_underlying(kind)].name;
}

} // mrdox
} // clang

