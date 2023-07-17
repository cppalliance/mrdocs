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

#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Support/TypeTraits.hpp>
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
    { "",                  "",                   "",    OperatorKind::None                },
    { "operator new",      "operator_new",       "nw",  OperatorKind::New                 },
    { "operator delete",   "operator_del",       "dl",  OperatorKind::Delete              },
    { "operator new[]",    "operator_arr_new",   "na",  OperatorKind::ArrayNew            },
    { "operator delete[]", "operator_arr_del",   "da",  OperatorKind::ArrayDelete         },
    { "operator+",         "operator_plus",      "pl",  OperatorKind::Plus                },
    { "operator-",         "operator_minus",     "mi",  OperatorKind::Minus               },
    { "operator*",         "operator_star",      "ml",  OperatorKind::Star                },
    { "operator/",         "operator_slash",     "dv",  OperatorKind::Slash               },
    { "operator%",         "operator_mod",       "rm",  OperatorKind::Percent             },
    { "operator^",         "operator_xor",       "eo",  OperatorKind::Caret               },
    { "operator&",         "operator_bitand",    "an",  OperatorKind::Amp                 },
    { "operator|",         "operator_bitor",     "or",  OperatorKind::Pipe                },
    { "operator~",         "operator_bitnot",    "co",  OperatorKind::Tilde               },
    { "operator!",         "operator_not",       "nt",  OperatorKind::Exclaim             },
    { "operator=",         "operator_assign",    "as",  OperatorKind::Equal               },
    { "operator<",         "operator_lt",        "lt",  OperatorKind::Less                },
    { "operator>",         "operator_gt",        "gt",  OperatorKind::Greater             },
    { "operator+=",        "operator_plus_eq",   "ple", OperatorKind::PlusEqual           },
    { "operator-=",        "operator_minus_eq",  "mie", OperatorKind::MinusEqual          },
    { "operator*=",        "operator_star_eq",   "mle", OperatorKind::StarEqual           },
    { "operator/=",        "operator_slash_eq",  "dve", OperatorKind::SlashEqual          },
    { "operator%=",        "operator_mod_eq",    "rme", OperatorKind::PercentEqual        },
    { "operator^=",        "operator_xor_eq",    "eoe", OperatorKind::CaretEqual          },
    { "operator&=",        "operator_and_eq",    "ane", OperatorKind::AmpEqual            },
    { "operator|=",        "operator_or_eq",     "ore", OperatorKind::PipeEqual           },
    { "operator<<",        "operator_lshift",    "ls",  OperatorKind::LessLess            },
    { "operator>>",        "operator_rshift",    "rs",  OperatorKind::GreaterGreater      },
    { "operator<<=",       "operator_lshift_eq", "lse", OperatorKind::LessLessEqual       },
    { "operator>>=",       "operator_rshift_eq", "rse", OperatorKind::GreaterGreaterEqual },
    { "operator==",        "operator_eq",        "eq",  OperatorKind::EqualEqual          },
    { "operator!=",        "operator_not_eq",    "ne",  OperatorKind::ExclaimEqual        },
    { "operator<=",        "operator_le",        "le",  OperatorKind::LessEqual           },
    { "operator>=",        "operator_ge",        "ge",  OperatorKind::GreaterEqual        },
    { "operator<=>",       "operator_3way",      "ss",  OperatorKind::Spaceship           },
    { "operator&&",        "operator_and",       "aa",  OperatorKind::AmpAmp              },
    { "operator||",        "operator_or",        "oo",  OperatorKind::PipePipe            },
    { "operator++",        "operator_inc",       "pp",  OperatorKind::PlusPlus            },
    { "operator--",        "operator_dec",       "mm",  OperatorKind::MinusMinus          },
    { "operator,",         "operator_comma",     "cm",  OperatorKind::Comma               },
    { "operator->*",       "operator_ptrmem",    "pm",  OperatorKind::ArrowStar           },
    { "operator->",        "operator_ptr",       "pt",  OperatorKind::Arrow               },
    { "operator()",        "operator_call",      "cl",  OperatorKind::Call                },
    { "operator[]",        "operator_subs",      "ix",  OperatorKind::Subscript           },
    { "operator?",         "operator_ternary",   "qu",  OperatorKind::Conditional         },
    { "operator co_await", "operator_coawait",   "ca",  OperatorKind::Coawait             },
    // { "~",         "dt",  FunctionKind::Destructor },
    // { "",          "ct",  FunctionKind::Constructor },
    // { "",          "cv",  FunctionKind::Conversion }
};

} // (anon)


std::string_view
getOperatorName(
    OperatorKind kind,
    bool include_keyword) noexcept
{
    MRDOX_ASSERT(Table[to_underlying(kind)].kind == kind);
    std::string_view full =
        Table[to_underlying(kind)].name;
    if(include_keyword || kind == OperatorKind::None)
        return full;
    // remove "operator"
    full.remove_prefix(8);
    // remove the space, if any
    if(full.front() == ' ')
        full.remove_prefix(1);
    return full;
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
    OperatorKind kind,
    bool include_keyword) noexcept
{
    MRDOX_ASSERT(Table[to_underlying(kind)].kind == kind);
    std::string_view full = Table[to_underlying(kind)].safe_name;
    if(include_keyword || kind == OperatorKind::None)
        return full;
    // remove "operator_"
    return full.substr(9);
}

dom::String
toString(
    FunctionClass kind) noexcept
{
    switch(kind)
    {
    case FunctionClass::Normal:
        return "normal";
    case FunctionClass::Constructor:
        return "constructor";
    case FunctionClass::Conversion:
        return "conversion";
    case FunctionClass::Deduction:
        return "deduction";
    case FunctionClass::Destructor:
        return "destructor";
    default:
        MRDOX_UNREACHABLE();
    }
}

} // mrdox
} // clang

