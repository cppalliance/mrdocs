//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <utility>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Info/Function.hpp>
#include <mrdocs/Support/TypeTraits.hpp>

namespace clang::mrdocs {

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
    case FunctionClass::Destructor:
        return "destructor";
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
merge(Param& I, Param&& Other)
{
    if (!I.Type)
    {
        I.Type = std::move(Other.Type);
    }
    if (!I.Name)
    {
        I.Name = std::move(Other.Name);
    }
    if (!I.Default)
    {
        I.Default = std::move(Other.Default);
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
    io.map("name", dom::stringOrNull(*p.Name));
    io.map("type", p.Type);
    io.map("default", dom::stringOrNull(*p.Default));
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
FunctionInfo::
operator<=>(FunctionInfo const& other) const
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
    return dynamic_cast<Info const&>(*this) <=> dynamic_cast<Info const&>(other);
}

void
merge(FunctionInfo& I, FunctionInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(dynamic_cast<Info&>(I), std::move(dynamic_cast<Info&>(Other)));
    if (I.Class == FunctionClass::Normal)
    {
        I.Class = Other.Class;
    }
    if (!I.ReturnType)
    {
        I.ReturnType = std::move(Other.ReturnType);
    }
    std::size_t const n = std::min(I.Params.size(), Other.Params.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        merge(I.Params[i], std::move(Other.Params[i]));
    }
    if (Other.Params.size() > n)
    {
        I.Params.insert(
            I.Params.end(),
            std::make_move_iterator(Other.Params.begin() + n),
            std::make_move_iterator(Other.Params.end()));
    }
    if (!I.Template)
    {
        I.Template = std::move(Other.Template);
    }
    else if (Other.Template)
    {
        merge(*I.Template, std::move(*Other.Template));
    }
    if (I.Noexcept.Implicit)
    {
        I.Noexcept = std::move(Other.Noexcept);
    }
    if (I.Explicit.Implicit)
    {
        I.Explicit = std::move(Other.Explicit);
    }
    merge(I.Requires, std::move(Other.Requires));
    I.IsVariadic |= Other.IsVariadic;
    I.IsVirtual |= Other.IsVirtual;
    I.IsVirtualAsWritten |= Other.IsVirtualAsWritten;
    I.IsPure |= Other.IsPure;
    I.IsDefaulted |= Other.IsDefaulted;
    I.IsExplicitlyDefaulted |= Other.IsExplicitlyDefaulted;
    I.IsDeleted |= Other.IsDeleted;
    I.IsDeletedAsWritten |= Other.IsDeletedAsWritten;
    I.HasTrailingReturn |= Other.HasTrailingReturn;
    I.IsConst |= Other.IsConst;
    I.IsVolatile |= Other.IsVolatile;
    I.IsFinal |= Other.IsFinal;
    I.IsOverride |= Other.IsOverride;
    I.IsExplicitObjectMemberFunction |= Other.IsExplicitObjectMemberFunction;
    if (I.Constexpr == ConstexprKind::None)
    {
        I.Constexpr = Other.Constexpr;
    }
    if (I.StorageClass == StorageClassKind::None)
    {
        I.StorageClass = Other.StorageClass;
    }
    if (I.RefQualifier == ReferenceKind::None)
    {
        I.RefQualifier = Other.RefQualifier;
    }
    if (I.OverloadedOperator == OperatorKind::None)
    {
        I.OverloadedOperator = Other.OverloadedOperator;
    }
}

MRDOCS_DECL
bool
overrides(FunctionInfo const& base, FunctionInfo const& derived)
{
    auto toOverrideTuple = [](FunctionInfo const& f) {
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

} // clang::mrdocs

