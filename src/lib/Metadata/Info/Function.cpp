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

#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Info/Function.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <utility>

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

dom::String
toString(FunctionClass const kind) noexcept
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
    return this->asInfo() <=> other.asInfo();
}

void
merge(FunctionInfo& I, FunctionInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(I.asInfo(), std::move(Other.asInfo()));
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
    I.IsNoReturn |= Other.IsNoReturn;
    I.HasOverrideAttr |= Other.HasOverrideAttr;
    I.HasTrailingReturn |= Other.HasTrailingReturn;
    I.IsConst |= Other.IsConst;
    I.IsVolatile |= Other.IsVolatile;
    I.IsFinal |= Other.IsFinal;
    I.IsNodiscard |= Other.IsNodiscard;
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

