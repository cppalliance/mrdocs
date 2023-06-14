//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/TypeTraits.hpp"
#include <mrdox/Metadata/Function.hpp>
#include <utility>

namespace clang {
namespace mrdox {

namespace {

struct Item
{
    char const* name;
    char const* safe_name;
    char const* short_name;
    OperatorKind kind;
};

// short operator names from:
// http://www.int0x80.gr/papers/name_mangling.pdf
static constinit Item const Table[] = {
    /*
        ::= ps # + (unary)
        ::= ng # - (unary)
        ::= ad # & (unary)
        ::= de # * (unary)
    */
    { "",         "",          "",    OperatorKind::None                },
    { "new",      "new",       "nw",  OperatorKind::New                 },
    { "delete",   "del",       "dl",  OperatorKind::Delete              },
    { "new[]",    "arr_new",   "na",  OperatorKind::ArrayNew            },
    { "delete[]", "arr_del",   "da",  OperatorKind::ArrayDelete         },
    { "+",        "plus",      "pl",  OperatorKind::Plus                },
    { "-",        "minus",     "mi",  OperatorKind::Minus               },
    { "*",        "star",      "ml",  OperatorKind::Star                },
    { "/",        "slash",     "dv",  OperatorKind::Slash               },
    { "%",        "mod",       "rm",  OperatorKind::Percent             },
    { "^",        "xor",       "eo",  OperatorKind::Caret               },
    { "&",        "bitand",    "an",  OperatorKind::Amp                 },
    { "|",        "bitor",     "or",  OperatorKind::Pipe                },
    { "~",        "bitnot",    "co",  OperatorKind::Tilde               },
    { "!",        "not",       "nt",  OperatorKind::Exclaim             },
    { "=",        "assign",    "as",  OperatorKind::Equal               },
    { "<",        "lt",        "lt",  OperatorKind::Less                },
    { ">",        "gt",        "gt",  OperatorKind::Greater             },
    { "+=",       "plus_eq",   "ple", OperatorKind::PlusEqual           },
    { "-=",       "minus_eq",  "mie", OperatorKind::MinusEqual          },
    { "*=",       "star_eq",   "mle", OperatorKind::StarEqual           },
    { "/=",       "slash_eq",  "dve", OperatorKind::SlashEqual          },
    { "%=",       "mod_eq",    "rme", OperatorKind::PercentEqual        },
    { "^=",       "xor_eq",    "eoe", OperatorKind::CaretEqual          },
    { "&=",       "and_eq",    "ane", OperatorKind::AmpEqual            },
    { "|=",       "or_eq",     "ore", OperatorKind::PipeEqual           },
    { "<<",       "lshift",    "ls",  OperatorKind::LessLess            },
    { ">>",       "rshift",    "rs",  OperatorKind::GreaterGreater      },
    { "<<=",      "lshift_eq", "lse", OperatorKind::LessLessEqual       },
    { ">>=",      "rshift_eq", "rse", OperatorKind::GreaterGreaterEqual },
    { "==",       "eq",        "eq",  OperatorKind::EqualEqual          },
    { "!=",       "not_eq",    "ne",  OperatorKind::ExclaimEqual        },
    { "<=",       "le",        "le",  OperatorKind::LessEqual           },
    { ">=",       "ge",        "ge",  OperatorKind::GreaterEqual        },
    { "<=>",      "3way",      "ss",  OperatorKind::Spaceship           },
    { "&&",       "and",       "aa",  OperatorKind::AmpAmp              },
    { "||",       "or",        "oo",  OperatorKind::PipePipe            },
    { "++",       "inc",       "pp",  OperatorKind::PlusPlus            },
    { "--",       "dec",       "mm",  OperatorKind::MinusMinus          },
    { ",",        "comma",     "cm",  OperatorKind::Comma               },
    { "->*",      "ptrmem",    "pm",  OperatorKind::ArrowStar           },
    { "->",       "ptr",       "pt",  OperatorKind::Arrow               },
    { "()",       "call",      "cl",  OperatorKind::Call                },
    { "[]",       "subs",      "ix",  OperatorKind::Subscript           },
    { "?",        "ternary",   "qu",  OperatorKind::Conditional         },
    { "co_await", "coawait",   "ca",  OperatorKind::Coawait             },
    // { "~",         "dt",  FunctionKind::Destructor },
    // { "",          "ct",  FunctionKind::Constructor },
    // { "",          "cv",  FunctionKind::Conversion }
};

} // (anon)


std::string_view
getOperatorName(
    OperatorKind kind) noexcept
{
    MRDOX_ASSERT(Table[to_underlying(kind)].kind == kind);
    return Table[to_underlying(kind)].name;
}

std::string_view
getShortOperatorName(
    OperatorKind kind) noexcept
{
    MRDOX_ASSERT(Table[to_underlying(kind)].kind == kind);
    return Table[to_underlying(kind)].short_name;
}

std::string_view
getSafeOperatorName(
    OperatorKind kind) noexcept
{
    MRDOX_ASSERT(Table[to_underlying(kind)].kind == kind);
    return Table[to_underlying(kind)].safe_name;
}

} // mrdox
} // clang

