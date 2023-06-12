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
    // empty state, only used when constructing TParams
    None,
    // template type parameter, e.g. "typename T" or "class T"
    Type,
    // template non-type parameter, e.g. "int N" or "auto N"
    NonType,
    // template template parameter, e.g. "template<typename> typename T"
    Template
};

struct TParam;

struct TypeTParam
{
    /** Default type for the type template parameter */
    std::optional<TypeInfo> Default;
};

struct NonTypeTParam
{
    /** Type of the non-type template parameter */
    TypeInfo Type;
    // Non-type template parameter default value (if any)
    Optional<std::string> Default;
};

struct TemplateTParam
{
    /** Template parameters for the template template parameter */
    std::vector<TParam> Params;
    /** Non-type template parameter default value (if any) */
    Optional<std::string> Default;
};

// ----------------------------------------------------------------

struct TParam
{
private:
    union Variant
    {
        Variant() { }
        ~Variant() { }

        TypeTParam Type;
        NonTypeTParam NonType;
        TemplateTParam Template;
    } Variant_;

    void destroy() const noexcept;

    template<typename T>
    void
    construct(T&& other)
    {
        switch(other.Kind)
        {
        case TParamKind::Type:
            emplace<TypeTParam>(
                std::forward<T>(other).Variant_.Type);
            break;
        case TParamKind::NonType:
            emplace<NonTypeTParam>(
                std::forward<T>(other).Variant_.NonType);
            break;
        case TParamKind::Template:
            emplace<TemplateTParam>(
                std::forward<T>(other).Variant_.Template);
            break;
        default:
            break;
        }
    }

public:
    /** The kind of template parameter this is. */
    TParamKind Kind = TParamKind::None;
    /** The template parameters name, if any */
    std::string Name;
    /** Whether this template parameter is a parameter pack. */
    bool IsParameterPack = false;


    TParam() = default;

    MRDOX_DECL
    TParam(
        TParam&& other) noexcept;

    MRDOX_DECL
    TParam(
        const TParam& other);

    MRDOX_DECL
    TParam(
        std::string&& name,
        bool is_pack);

    MRDOX_DECL ~TParam();

    MRDOX_DECL
    TParam&
    operator=(
        const TParam& other);

    MRDOX_DECL
    TParam&
    operator=(
        TParam&& other) noexcept;

    template<typename T, typename... Args>
    auto&
    emplace(Args&&... args)
    {
        using U = std::remove_cvref_t<T>;
        if constexpr(std::is_same_v<U, TypeTParam>)
            Kind = TParamKind::Type;
        else if constexpr(std::is_same_v<U, NonTypeTParam>)
            Kind = TParamKind::NonType;
        else if constexpr(std::is_same_v<U, TemplateTParam>)
            Kind = TParamKind::Template;
        else
            MRDOX_ASSERT(! "invalid template parameter kind");
        return *::new (static_cast<void*>(&Variant_)) T(
            std::forward<Args>(args)...);
    }

    template<typename T>
    auto& get() noexcept
    {
        using U = std::remove_cvref_t<T>;
        if constexpr(std::is_same_v<U, TypeTParam>)
            return Variant_.Type;
        else if constexpr(std::is_same_v<U, NonTypeTParam>)
            return Variant_.NonType;
        else if constexpr(std::is_same_v<U, TemplateTParam>)
            return Variant_.Template;
        else
            MRDOX_ASSERT(! "invalid template parameter kind");
    }

    template<typename T>
    const auto& get() const noexcept
    {
        using U = std::remove_cvref_t<T>;
        if constexpr(std::is_same_v<U, TypeTParam>)
            return Variant_.Type;
        else if constexpr(std::is_same_v<U, NonTypeTParam>)
            return Variant_.NonType;
        else if constexpr(std::is_same_v<U, TemplateTParam>)
            return Variant_.Template;
        else
            MRDOX_ASSERT(! "invalid template parameter kind");
    }
};

// ----------------------------------------------------------------

struct TArg
{
    std::string Value;

    TArg() = default;

    MRDOX_DECL
    TArg(std::string&& value);
};

// ----------------------------------------------------------------


// ----------------------------------------------------------------

enum class TemplateSpecKind
{
    Primary = 0, // for bitstream
    Explicit,
    Partial
};

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
    std::vector<TParam> Params;
    std::vector<TArg> Args;

    /** Primary template ID for partial and explicit specializations.
    */
    OptionalSymbolID Primary;

#if 0
    /** Stores information for explicit specializations of members
        of implicitly instantiated class template specializations
    */
    std::vector<SpecializationInfo> Specializations;
#endif

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
