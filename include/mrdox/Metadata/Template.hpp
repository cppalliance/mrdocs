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

#ifndef MRDOX_METADATA_TEMPLATE_HPP
#define MRDOX_METADATA_TEMPLATE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <string>

namespace clang {
namespace mrdox {

enum class TParamKind : int
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
    // The kind of template parameter this is.
    TParamKind Kind = TParamKind::None;
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
            Kind = TParamKind::Type;
        else if constexpr(std::is_same_v<U, NonTypeTParam>)
            Kind = TParamKind::NonType;
        else if constexpr(std::is_same_v<U, TemplateTParam>)
            Kind = TParamKind::Template;
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

// ----------------------------------------------------------------

struct TArg
{
    std::string Value;

    TArg() = default;

    MRDOX_DECL
    TArg(std::string&& value);
};

// ----------------------------------------------------------------

// stores information pertaining to an explicit specialization of a
// member of an implicitly instantiated class template specialization.
// this structure is stored in the TemplateInfo corresponding to the
// outermost specialized template. if the explicitly specialized
// member is itself a member of a nested template, the SpecializationInfo
// which stores the arguments for the parent template will additionally
// store a pointer to the SpecializationInfo for the nested template,
// recursively. each SpecializationInfo node contains a vector of
// SymbolIDs pairs `(specialized, primary)`, where `specialized` is the
// replacement definition of `primary` for the given set of template
// arguments of its parent template(s).
struct SpecializationInfo
{
    // template arguments a parent template is specialized for
    std::vector<TArg> Args;

    // ID of the template to which the arguments pertain
    SymbolID Template;

    // SpecializationInfo for nested templates which are also specialized
    std::vector<SpecializationInfo> Nested;

    // SymbolID pairs of any specialized members.
    // the first element is the ID of the original member (i.e. member that is replaced)
    // the second element is the ID of the specialized member (i.e. replacement)
    std::vector<std::pair<SymbolID, SymbolID>> Members;
};

// ----------------------------------------------------------------

enum class TemplateSpecKind
{
    Primary = 0, // for bitstream
    Explicit,
    Partial
};

// stores information pertaining to primary template and
// partial/explicit specialization declarations
struct TemplateInfo
{
    // for primary templates:
    //     - Params will be non-empty
    //     - Args will be empty
    // for explicit specializations:
    //     - Params will be empty
    // for partial specializations:
    //     - Params will be non-empty
    //     - each template parameter will appear at least
    //       once in Args outside of a non-deduced context
    std::vector<TParam> Params;
    std::vector<TArg> Args;

    // stores the ID of the corresponding primary template
    // for partial and explicit specializations
    OptionalSymbolID Primary;

    // stores information for explicit specializations of members
    // of implicitly instantiated class template specializations
    std::vector<SpecializationInfo> Specializations;

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
