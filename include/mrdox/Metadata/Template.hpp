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

#ifndef MRDOX_API_METADATA_TEMPLATE_HPP
#define MRDOX_API_METADATA_TEMPLATE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/ADT/Optional.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <optional>
#include <string>
#include <vector>

namespace clang {
namespace mrdox {

enum class TParamKind : int
{
    // template type parameter, e.g. "typename T" or "class T"
    Type = 1, // for bitstream
    // template non-type parameter, e.g. "int N" or "auto N"
    NonType,
    // template template parameter, e.g. "template<typename> typename T"
    Template
};

MRDOX_DECL dom::String toString(TParamKind kind) noexcept;

struct TParam
{
    /** The kind of template parameter this is. */
    TParamKind Kind;

    /** The template parameters name, if any */
    std::string Name;

    /** Whether this template parameter is a parameter pack. */
    bool IsParameterPack = false;

    constexpr virtual ~TParam() = default;

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

struct TypeTParam
    : IsTParam<TParamKind::Type>
{
    /** Default type for the type template parameter */
    std::unique_ptr<TypeInfo> Default;
};

struct NonTypeTParam
    : IsTParam<TParamKind::NonType>
{
    /** Type of the non-type template parameter */
    std::unique_ptr<TypeInfo> Type;
    // Non-type template parameter default value (if any)
    Optional<std::string> Default;
};

struct TemplateTParam
    : IsTParam<TParamKind::Template>
{
    /** Template parameters for the template template parameter */
    std::vector<std::unique_ptr<TParam>> Params;
    /** Non-type template parameter default value (if any) */
    Optional<std::string> Default;
};

template<typename F, typename... Args>
constexpr
decltype(auto)
visit(
    TParam& P,
    F&& f,
    Args&&... args)
{
    switch(P.Kind)
    {
    case TParamKind::Type:
        return f(static_cast<TypeTParam&>(P),
            std::forward<Args>(args)...);
    case TParamKind::NonType:
        return f(static_cast<NonTypeTParam&>(P),
            std::forward<Args>(args)...);
    case TParamKind::Template:
        return f(static_cast<TemplateTParam&>(P),
            std::forward<Args>(args)...);
    default:
        MRDOX_UNREACHABLE();
    }
}

template<typename F, typename... Args>
constexpr
decltype(auto)
visit(
    const TParam& P,
    F&& f,
    Args&&... args)
{
    switch(P.Kind)
    {
    case TParamKind::Type:
        return f(static_cast<const TypeTParam&>(P),
            std::forward<Args>(args)...);
    case TParamKind::NonType:
        return f(static_cast<const NonTypeTParam&>(P),
            std::forward<Args>(args)...);
    case TParamKind::Template:
        return f(static_cast<const TemplateTParam&>(P),
            std::forward<Args>(args)...);
    default:
        MRDOX_UNREACHABLE();
    }
}

// ----------------------------------------------------------------

struct TArg
{
    std::string Value;

    TArg() = default;

    MRDOX_DECL
    TArg(std::string&& value);
};

// ----------------------------------------------------------------

enum class TemplateSpecKind
{
    Primary = 0, // for bitstream
    Explicit,
    Partial
};

MRDOX_DECL
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
    std::vector<TArg> Args;

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

} // mrdox
} // clang

#endif
