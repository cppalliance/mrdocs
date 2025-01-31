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

#ifndef MRDOCS_API_METADATA_TEMPLATE_HPP
#define MRDOCS_API_METADATA_TEMPLATE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <optional>
#include <string>
#include <vector>

namespace clang::mrdocs {

std::strong_ordering
operator<=>(PolymorphicValue<TParam> const& lhs, PolymorphicValue<TParam> const& rhs);

enum class TArgKind : int
{
    // type arguments
    Type = 1, // for bitstream
    // non-type arguments, i.e. expressions
    NonType,
    // template template arguments, i.e. template names
    Template
};

MRDOCS_DECL std::string_view toString(TArgKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TArgKind kind)
{
    v = toString(kind);
}

struct TArg
{
    /** The kind of template argument this is. */
    TArgKind Kind;

    /** Whether this template argument is a parameter expansion. */
    bool IsPackExpansion = false;

    constexpr virtual ~TArg() = default;

    constexpr bool isType()     const noexcept { return Kind == TArgKind::Type; }
    constexpr bool isNonType()  const noexcept { return Kind == TArgKind::NonType; }
    constexpr bool isTemplate() const noexcept { return Kind == TArgKind::Template; }

    auto operator<=>(TArg const&) const = default;

protected:
    constexpr
    TArg(
        TArgKind kind) noexcept
        : Kind(kind)
    {
    }
};

template<TArgKind K>
struct IsTArg : TArg
{
    static constexpr TArgKind kind_id = K;

    static constexpr bool isType()     noexcept { return K == TArgKind::Type; }
    static constexpr bool isNonType()  noexcept { return K == TArgKind::NonType; }
    static constexpr bool isTemplate() noexcept { return K == TArgKind::Template; }

protected:
    constexpr
    IsTArg() noexcept
        : TArg(K)
    {
    }
};

struct TypeTArg final
    : IsTArg<TArgKind::Type>
{
    /** Template argument type. */
    PolymorphicValue<TypeInfo> Type;

    auto operator<=>(TypeTArg const&) const = default;
};


struct NonTypeTArg final
    : IsTArg<TArgKind::NonType>
{
    /** Template argument expression. */
    ExprInfo Value;

    auto operator<=>(NonTypeTArg const&) const = default;
};

struct TemplateTArg final
    : IsTArg<TArgKind::Template>
{
    /** SymbolID of the referenced template. */
    SymbolID Template;

    /** Name of the referenced template. */
    std::string Name;

    auto operator<=>(TemplateTArg const&) const = default;
};

template<
    std::derived_from<TArg> TArgTy,
    class F,
    class... Args>
constexpr
decltype(auto)
visit(
    TArgTy& A,
    F&& f,
    Args&&... args)
{
    switch(A.Kind)
    {
    case TArgKind::Type:
        return f(static_cast<add_cv_from_t<
            TArgTy, TypeTArg>&>(A),
            std::forward<Args>(args)...);
    case TArgKind::NonType:
        return f(static_cast<add_cv_from_t<
            TArgTy, NonTypeTArg>&>(A),
            std::forward<Args>(args)...);
    case TArgKind::Template:
        return f(static_cast<add_cv_from_t<
            TArgTy, TemplateTArg>&>(A),
            std::forward<Args>(args)...);
    default:
        MRDOCS_UNREACHABLE();
    }
}

inline
std::strong_ordering
operator<=>(PolymorphicValue<TArg> const& lhs, PolymorphicValue<TArg> const& rhs)
{
    return CompareDerived(lhs, rhs);
}

inline
bool
operator==(PolymorphicValue<TArg> const& lhs, PolymorphicValue<TArg> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}

MRDOCS_DECL
std::string
toString(TArg const& arg) noexcept;

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TArg const& I,
    DomCorpus const* domCorpus);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    PolymorphicValue<TArg> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

// ----------------------------------------------------------------

enum class TParamKind : int
{
    // template type parameter, e.g. "typename T" or "class T"
    Type = 1, // for bitstream
    // template non-type parameter, e.g. "int N" or "auto N"
    NonType,
    // Template-template parameter, e.g. "template<typename> typename T"
    Template
};

MRDOCS_DECL std::string_view toString(TParamKind kind) noexcept;

struct TParam
{
    /** The kind of template parameter this is */
    TParamKind Kind;

    /** The template parameters name, if any */
    std::string Name;

    /** Whether this template parameter is a parameter pack */
    bool IsParameterPack = false;

    /** The default template argument, if any */
    PolymorphicValue<TArg> Default;

    constexpr virtual ~TParam() = default;

    constexpr bool isType()     const noexcept { return Kind == TParamKind::Type; }
    constexpr bool isNonType()  const noexcept { return Kind == TParamKind::NonType; }
    constexpr bool isTemplate() const noexcept { return Kind == TParamKind::Template; }

    auto operator<=>(TParam const&) const = default;

protected:
    constexpr
    TParam(
        TParamKind kind) noexcept
        : Kind(kind)
    {
    }
};

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TParam const& I,
    DomCorpus const* domCorpus);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    PolymorphicValue<TParam> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}


template<TParamKind K>
struct TParamCommonBase : TParam
{
    static constexpr TParamKind kind_id = K;

    static constexpr bool isType()     noexcept { return K == TParamKind::Type; }
    static constexpr bool isNonType()  noexcept { return K == TParamKind::NonType; }
    static constexpr bool isTemplate() noexcept { return K == TParamKind::Template; }

    auto operator<=>(TParamCommonBase const&) const = default;

protected:
    constexpr
    TParamCommonBase() noexcept
        : TParam(K)
    {
    }
};

/** The keyword a template parameter was declared with */
enum class TParamKeyKind : int
{
    Class = 0,
    Typename
};

MRDOCS_DECL std::string_view toString(TParamKeyKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TParamKeyKind kind)
{
    v = toString(kind);
}

struct TypeTParam final
    : TParamCommonBase<TParamKind::Type>
{
    /** Keyword (class/typename) the parameter uses */
    TParamKeyKind KeyKind = TParamKeyKind::Class;

    /** The type-constraint for the parameter, if any. */
    PolymorphicValue<NameInfo> Constraint;

    auto operator<=>(TypeTParam const&) const = default;
};

struct NonTypeTParam final
    : TParamCommonBase<TParamKind::NonType>
{
    /** Type of the non-type template parameter */
    PolymorphicValue<TypeInfo> Type;

    auto operator<=>(NonTypeTParam const&) const = default;
};

struct TemplateTParam final
    : TParamCommonBase<TParamKind::Template>
{
    /** Template parameters for the template-template parameter */
    std::vector<PolymorphicValue<TParam>> Params;

    auto operator<=>(TemplateTParam const& other) const
    {
        if (auto const r = Params.size() <=> other.Params.size();
            !std::is_eq(r))
        {
            return r;
        }
        for (std::size_t i = 0; i < Params.size(); ++i)
        {
            if (auto const r = Params[i] <=> other.Params[i];
                !std::is_eq(r))
            {
                return r;
            }
        }
        return std::strong_ordering::equal;
    }
};

template<
    typename TParamTy,
    typename F,
    typename... Args>
    requires std::derived_from<TParamTy, TParam>
constexpr
decltype(auto)
visit(
    TParamTy& P,
    F&& f,
    Args&&... args)
{
    switch(P.Kind)
    {
    case TParamKind::Type:
        return f(static_cast<add_cv_from_t<
            TParamTy, TypeTParam>&>(P),
            std::forward<Args>(args)...);
    case TParamKind::NonType:
        return f(static_cast<add_cv_from_t<
            TParamTy, NonTypeTParam>&>(P),
            std::forward<Args>(args)...);
    case TParamKind::Template:
        return f(static_cast<add_cv_from_t<
            TParamTy, TemplateTParam>&>(P),
            std::forward<Args>(args)...);
    default:
        MRDOCS_UNREACHABLE();
    }
}

inline
std::strong_ordering
operator<=>(PolymorphicValue<TParam> const& lhs, PolymorphicValue<TParam> const& rhs)
{
    return CompareDerived(lhs, rhs);
}

inline
bool
operator==(PolymorphicValue<TParam> const& lhs, PolymorphicValue<TParam> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}


// ----------------------------------------------------------------

enum class TemplateSpecKind
{
    Primary,
    Explicit,
    Partial
};

MRDOCS_DECL
std::string_view
toString(TemplateSpecKind kind);

/** Information pertaining to templates and specializations thereof.
*/
struct TemplateInfo
{
    std::vector<PolymorphicValue<TParam>> Params;
    std::vector<PolymorphicValue<TArg>> Args;

    /** The requires-clause for the template parameter list, if any.
    */
    ExprInfo Requires;

    /** Primary template ID for partial and explicit specializations.
    */
    SymbolID Primary = SymbolID::invalid;

    // KRYSTIAN NOTE: using the presence of args/params
    // to determine the specialization kind *should* work.
    // emphasis on should.
    TemplateSpecKind
    specializationKind() const noexcept
    {
        if (Params.empty())
        {
            return TemplateSpecKind::Explicit;
        }
        if (Args.empty())
        {
            return TemplateSpecKind::Primary;
        }
        return TemplateSpecKind::Partial;
    }

    auto operator<=>(TemplateInfo const&) const = default;
};

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TemplateInfo const& I,
    DomCorpus const* domCorpus);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    std::optional<TemplateInfo> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

} // clang::mrdocs

#endif
