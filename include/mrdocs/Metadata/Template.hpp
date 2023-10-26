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

namespace clang {
namespace mrdocs {

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

struct TypeTArg
    : IsTArg<TArgKind::Type>
{
    /** Template argument type. */
    std::unique_ptr<TypeInfo> Type;
};

struct NonTypeTArg
    : IsTArg<TArgKind::NonType>
{
    /** Template argument expression. */
    ExprInfo Value;
};

struct TemplateTArg
    : IsTArg<TArgKind::Template>
{
    /** SymbolID of the referenced template. */
    SymbolID Template;

    /** Name of the referenced template. */
    std::string Name;
};

template<
    typename TArgTy,
    typename F,
    typename... Args>
    requires std::derived_from<TArgTy, TArg>
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

MRDOCS_DECL std::string toString(const TArg& arg) noexcept;

// ----------------------------------------------------------------

enum class TParamKind : int
{
    // template type parameter, e.g. "typename T" or "class T"
    Type = 1, // for bitstream
    // template non-type parameter, e.g. "int N" or "auto N"
    NonType,
    // template template parameter, e.g. "template<typename> typename T"
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
    std::unique_ptr<TArg> Default;

    virtual ~TParam() = default;

    constexpr bool isType()     const noexcept { return Kind == TParamKind::Type; }
    constexpr bool isNonType()  const noexcept { return Kind == TParamKind::NonType; }
    constexpr bool isTemplate() const noexcept { return Kind == TParamKind::Template; }

protected:
    constexpr
    TParam(
        TParamKind kind) noexcept
        : Kind(kind)
    {
    }
};

template<TParamKind K>
struct IsTParam : TParam
{
    static constexpr TParamKind kind_id = K;

    static constexpr bool isType()     noexcept { return K == TParamKind::Type; }
    static constexpr bool isNonType()  noexcept { return K == TParamKind::NonType; }
    static constexpr bool isTemplate() noexcept { return K == TParamKind::Template; }

protected:
    constexpr
    IsTParam() noexcept
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

struct TypeTParam
    : IsTParam<TParamKind::Type>
{
    /** Keyword (class/typename) the parameter uses */
    TParamKeyKind KeyKind = TParamKeyKind::Class;
};

struct NonTypeTParam
    : IsTParam<TParamKind::NonType>
{
    /** Type of the non-type template parameter */
    std::unique_ptr<TypeInfo> Type;
};

struct TemplateTParam
    : IsTParam<TParamKind::Template>
{
    /** Template parameters for the template template parameter */
    std::vector<std::unique_ptr<TParam>> Params;
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

// ----------------------------------------------------------------

enum class TemplateSpecKind
{
    Primary = 0, // for bitstream
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
    /* For primary templates:
           - Params will be non-empty
           - Args will be empty
       For explicit specializations:
           - Params will be empty
       For partial specializations:
           - Params will be non-empty
           - each template parameter will appear at least
             once in Args outside of a non-deduced context
    */
    std::vector<std::unique_ptr<TParam>> Params;
    std::vector<std::unique_ptr<TArg>> Args;

    /** Primary template ID for partial and explicit specializations.
    */
    OptionalSymbolID Primary;

    // KRYSTIAN NOTE: using the presence of args/params
    // to determine the specialization kind *should* work.
    // emphasis on should.
    TemplateSpecKind specializationKind() const noexcept
    {
        if(Params.empty())
            return TemplateSpecKind::Explicit;
        return Args.empty() ? TemplateSpecKind::Primary :
            TemplateSpecKind::Partial;
    }
};

} // mrdocs
} // clang

#endif
