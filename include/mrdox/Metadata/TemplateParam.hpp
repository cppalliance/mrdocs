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

#ifndef MRDOX_METADATA_TEMPLATEPARAM_HPP
#define MRDOX_METADATA_TEMPLATEPARAM_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Type.hpp>

namespace clang {
namespace mrdox {

enum class TemplateParamKind : int
{
    None = 0, // for bitstream
    // template type parameter, e.g. "typename T" or "class T"
    Type,
    // template non-type parameter, e.g. "int N" or "auto N"
    NonType,
    // template template parameter, e.g. "template<typename> typename T"
    Template,
};

struct TParam;

struct TypeTParam
{
    // default type for the type template parameter
    std::optional<TypeInfo> Default;
};

struct NonTypeTParam
{
    // type of the non-type template parameter
    TypeInfo Type;
    // non-type template parameter default value (if any)
    std::optional<std::string> Default;
};

struct TemplateTParam
{
    // template parameters for the template template parameter
    std::vector<TParam> Params;
    // non-type template parameter default value (if any)
    std::optional<std::string> Default;
};


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
        case TemplateParamKind::Type:
            emplace<TypeTParam>(
                std::forward<T>(other).Variant_.Type);
            break;
        case TemplateParamKind::NonType:
            emplace<NonTypeTParam>(
                std::forward<T>(other).Variant_.NonType);
            break;
        case TemplateParamKind::Template:
            emplace<TemplateTParam>(
                std::forward<T>(other).Variant_.Template);
            break;
        default:
            break;
        }
    }

public:
    // The kind of template parameter this is.
    TemplateParamKind Kind = TemplateParamKind::None;
    // The template parameters name, if any
    std::string Name;
    // Whether this template parameter is a parameter pack.
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
            Kind = TemplateParamKind::Type;
        else if constexpr(std::is_same_v<U, NonTypeTParam>)
            Kind = TemplateParamKind::NonType;
        else if constexpr(std::is_same_v<U, TemplateTParam>)
            Kind = TemplateParamKind::Template;
        else
            assert(! "invalid template parameter kind");
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
            assert(! "invalid template parameter kind");
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
            assert(! "invalid template parameter kind");
    }
};

} // mrdox
} // clang

#endif
