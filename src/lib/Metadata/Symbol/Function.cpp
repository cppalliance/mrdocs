//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Reflection/MergeReflectedType.hpp>
#include <lib/Support/Reflection/Reflection.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Type/LValueReferenceType.hpp>
#include <mrdocs/Metadata/Type/RValueReferenceType.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <algorithm>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

namespace mrdocs {

namespace {

struct Item
{
    char const* name;
    char const* legible_name;
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
    { "operator=",         "operator_assign",    "as",  OperatorKind::Equal               },
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

    // relational operators
    { "operator!",         "operator_not",       "nt",  OperatorKind::Exclaim             },
    { "operator==",        "operator_eq",        "eq",  OperatorKind::EqualEqual          },
    { "operator!=",        "operator_not_eq",    "ne",  OperatorKind::ExclaimEqual        },
    { "operator<",         "operator_lt",        "lt",  OperatorKind::Less                },
    { "operator<=",        "operator_le",        "le",  OperatorKind::LessEqual           },
    { "operator>",         "operator_gt",        "gt",  OperatorKind::Greater             },
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

bool
isLValueReferenceToRecord(
    Param const& param,
    SymbolID recordId)
{
    Type const& ptype = *param.Type;
    if (!ptype.isLValueReference())
    {
        return false;
    }
    auto const& pointee = ptype.asLValueReference().PointeeType;
    return (*pointee).isNamed()
        && (*pointee).namedSymbol() == recordId;
}

bool
isRValueReferenceToRecord(
    Param const& param,
    SymbolID recordId)
{
    Type const& ptype = *param.Type;
    if (!ptype.isRValueReference())
    {
        return false;
    }
    auto const& pointee = ptype.asRValueReference().PointeeType;
    return (*pointee).isNamed()
        && (*pointee).namedSymbol() == recordId;
}

bool
hasAllDefaults(
    std::span<Param const> params)
{
    return std::ranges::all_of(params,
        [](Param const& p) { return p.Default.has_value(); });
}

} // (anon)


std::string_view
getOperatorName(
    OperatorKind const kind,
    bool include_keyword) noexcept
{
    MRDOCS_ASSERT(Table[to_underlying(kind)].kind == kind);
    std::string_view full =
        Table[to_underlying(kind)].name;
    if(include_keyword || kind == OperatorKind::None)
        return full;
    // remove "operator"
    full.remove_prefix(8);
    // remove the space, if any
    if (full.front() == ' ')
    {
        full.remove_prefix(1);
    }
    return full;
}

OperatorKind
getOperatorKind(std::string_view name) noexcept
{
    for(auto const& item : Table)
    {
        if(name == item.name)
        {
            return item.kind;
        }
    }
    return OperatorKind::None;
}

OperatorKind
getOperatorKindFromSuffix(std::string_view suffix) noexcept
{
    for(auto const& item : Table)
    {
        std::string_view itemSuffix = item.name;
        if (!itemSuffix.starts_with("operator"))
        {
            continue;
        }
        itemSuffix.remove_prefix(8);
        itemSuffix = ltrim(itemSuffix);
        if(suffix == itemSuffix)
        {
            return item.kind;
        }
    }
    return OperatorKind::None;
}


std::string_view
getShortOperatorName(
    OperatorKind kind) noexcept
{
    MRDOCS_ASSERT(Table[to_underlying(kind)].kind == kind);
    return Table[to_underlying(kind)].short_name;
}

std::string_view
getSafeOperatorName(
    OperatorKind kind,
    bool include_keyword) noexcept
{
    MRDOCS_ASSERT(Table[to_underlying(kind)].kind == kind);
    std::string_view full = Table[to_underlying(kind)].legible_name;
    if(include_keyword || kind == OperatorKind::None)
        return full;
    // remove "operator_"
    return full.substr(9);
}

Optional<std::string_view>
getOperatorReadableName(
    OperatorKind const kind,
    int const nParams)
{
    switch (kind)
    {
        case OperatorKind::Equal:
            return "Assignment";
        case OperatorKind::Star:
            return nParams != 2 ? "Dereference" : "Multiplication";
        case OperatorKind::Arrow:
            return "Member access";
        case OperatorKind::Exclaim:
            return "Negation";
        case OperatorKind::EqualEqual:
            return "Equality";
        case OperatorKind::ExclaimEqual:
            return "Inequality";
        case OperatorKind::Less:
            return "Less-than";
        case OperatorKind::LessEqual:
            return "Less-than-or-equal";
        case OperatorKind::Greater:
            return "Greater-than";
        case OperatorKind::GreaterEqual:
            return "Greater-than-or-equal";
        case OperatorKind::Spaceship:
            return "Three-way comparison";
        case OperatorKind::AmpAmp:
            return "Conjunction";
        case OperatorKind::PipePipe:
            return "Disjunction";
        case OperatorKind::PlusPlus:
            return "Increment";
        case OperatorKind::MinusMinus:
            return "Decrement";
        case OperatorKind::Comma:
            return "Comma";
        case OperatorKind::ArrowStar:
            return "Pointer-to-member";
        case OperatorKind::Call:
            return "Function call";
        case OperatorKind::Subscript:
            return "Subscript";
        case OperatorKind::Conditional:
            return "Ternary";
        case OperatorKind::Coawait:
            return "Coawait";
        case OperatorKind::New:
            return "New";
        case OperatorKind::Delete:
            return "Delete";
        case OperatorKind::ArrayNew:
            return "New array";
        case OperatorKind::ArrayDelete:
            return "Delete array";
        case OperatorKind::Plus:
            return nParams != 2 ? "Unary plus" : "Addition";
        case OperatorKind::Minus:
            return nParams != 2 ? "Unary minus" : "Subtraction";
        case OperatorKind::Slash:
            return "Division";
        case OperatorKind::Percent:
            return "Modulus";
        case OperatorKind::Pipe:
            return "Bitwise disjunction";
        case OperatorKind::Caret:
            return "Bitwise exclusive-or";
        case OperatorKind::Tilde:
            return "Bitwise negation";
        case OperatorKind::PlusEqual:
            return "Addition assignment";
        case OperatorKind::MinusEqual:
            return "Subtraction assignment";
        case OperatorKind::StarEqual:
            return "Multiplication assignment";
        case OperatorKind::SlashEqual:
            return "Division assignment";
        case OperatorKind::PercentEqual:
            return "Modulus assignment";
        case OperatorKind::Amp:
            return nParams != 2 ? "Address-of" : "Bitwise conjunction";
        case OperatorKind::AmpEqual:
            return "Bitwise conjunction assignment";
        case OperatorKind::PipeEqual:
            return "Bitwise disjunction assignment";
        case OperatorKind::CaretEqual:
            return "Bitwise exclusive-or assignment";
        case OperatorKind::LessLess:
            return "Left shift";
        case OperatorKind::GreaterGreater:
            return "Right shift";
        case OperatorKind::LessLessEqual:
            return "Left shift assignment";
        case OperatorKind::GreaterGreaterEqual:
            return "Right shift";
        default:
            return std::nullopt;
    }
    MRDOCS_UNREACHABLE();
}

void
merge(Param& I, Param&& Other)
{
    mergeReflected(I, Other);
}

void
merge(std::vector<Param>& dst, std::vector<Param>&& src)
{
    std::size_t const n = std::min(dst.size(), src.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        merge(dst[i], std::move(src[i]));
    }
    if (src.size() > n)
    {
        dst.insert(
            dst.end(),
            std::make_move_iterator(src.begin() + n),
            std::make_move_iterator(src.end()));
    }
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Param const& p,
    DomCorpus const*)
{
    io.map("name", dom::stringOrNull(p.Name));
    io.map("type", p.Type);
    io.map("default", dom::stringOrNull(p.Default));
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Param const& p,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(p, domCorpus);
}

std::strong_ordering
FunctionSymbol::
operator<=>(FunctionSymbol const& other) const
{
    if (auto const cmp = Name <=> other.Name;
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (auto const cmp = Params.size() <=> other.Params.size();
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (auto const cmp = Template.operator bool() <=> other.Template.operator bool();
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (Template && other.Template)
    {
        if (auto const cmp = Template->Args.size() <=> other.Template->Args.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        if (auto const cmp = Template->Params.size() <=> other.Template->Params.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    if (auto const cmp = Params <=> other.Params;
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (Template && other.Template)
    {
        if (auto const cmp = Template->Args <=> other.Template->Args;
            !std::is_eq(cmp))
        {
            return cmp;
        }
        if (auto const cmp = Template->Params <=> other.Template->Params;
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    return this->asInfo() <=> other.asInfo();
}

void
merge(FunctionSymbol& I, FunctionSymbol&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    mergeReflected(I, Other);
}

bool
isDefaultConstructor(FunctionSymbol const& func)
{
    return func.FuncClass == FunctionClass::Constructor
        && (func.Params.empty() ||
            std::ranges::all_of(func.Params,
                [](Param const& p) {
                    return p.Default.has_value()
                        || p.Type->IsPackExpansion;
                }));
}

bool
isCopyConstructor(FunctionSymbol const& func)
{
    if (func.FuncClass != FunctionClass::Constructor
        || func.Template
        || func.Params.empty())
    {
        return false;
    }
    return isLValueReferenceToRecord(func.Params[0], func.Parent)
        && hasAllDefaults(std::span(func.Params).subspan(1));
}

bool
isMoveConstructor(FunctionSymbol const& func)
{
    if (func.FuncClass != FunctionClass::Constructor
        || func.Template
        || func.Params.empty())
    {
        return false;
    }
    return isRValueReferenceToRecord(func.Params[0], func.Parent)
        && hasAllDefaults(std::span(func.Params).subspan(1));
}

bool
isCopyAssignment(FunctionSymbol const& func)
{
    if (func.Template
        || func.OverloadedOperator != OperatorKind::Equal)
    {
        return false;
    }
    if (isLValueReferenceToRecord(func.Params[0], func.Parent))
    {
        return true;
    }
    // Copy assignment by value: operator=(X).
    Type const& ptype = *func.Params[0].Type;
    return ptype.isNamed()
        && ptype.namedSymbol() == func.Parent;
}

bool
isMoveAssignment(FunctionSymbol const& func)
{
    if (func.Template
        || func.OverloadedOperator != OperatorKind::Equal)
    {
        return false;
    }
    return isRValueReferenceToRecord(func.Params[0], func.Parent);
}

bool
isSpecialMemberFunction(FunctionSymbol const& func)
{
    return func.FuncClass == FunctionClass::Destructor
        || isDefaultConstructor(func)
        || isCopyConstructor(func)
        || isMoveConstructor(func)
        || isCopyAssignment(func)
        || isMoveAssignment(func);
}

MRDOCS_DECL
bool
overrides(FunctionSymbol const& base, FunctionSymbol const& derived)
{
    auto toOverrideTuple = [](FunctionSymbol const& f) {
        return std::forward_as_tuple(
            f.Name,
            f.Params,
            f.Template,
            f.IsVariadic,
            f.IsConst,
            f.RefQualifier
        );
    };
    return toOverrideTuple(base) == toOverrideTuple(derived);
}

} // mrdocs

