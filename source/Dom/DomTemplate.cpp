//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomSymbol.hpp>
#include <mrdox/Dom/DomTemplate.hpp>
#include <mrdox/Dom/DomType.hpp>

namespace clang {
namespace mrdox {

// ----------------------------------------------------------------

dom::Value
DomTParam::
get(std::string_view key) const
{
    if(key == "kind")
    {
        switch(I_->Kind)
        {
        case TParamKind::Type:
            return "type";
        case TParamKind::NonType:
            return "non-type";
        case TParamKind::Template:
            return "template";
        default:
            MRDOX_UNREACHABLE();
        }
    }
    if(key == "name")
        return dom::nonEmptyString(I_->Name);
    if(key == "is-pack")
        return I_->IsParameterPack;
    if(key == "type")
    {
        if(I_->Kind != TParamKind::NonType)
            return nullptr;
        return dom::create<DomType>(
            I_->get<NonTypeTParam>().Type, corpus_);
    }
    if(key == "params")
    {
        if(I_->Kind != TParamKind::Template)
            return nullptr;
        return dom::create<DomTParamArray>(
            I_->get<TemplateTParam>().Params, corpus_);
    }
    if(key == "default")
    {
        switch(I_->Kind)
        {
        case TParamKind::Type:
        {
            const auto& P = I_->get<TypeTParam>();
            if(! P.Default)
                return nullptr;
            return dom::create<DomType>(
                *P.Default, corpus_);
        }
        case TParamKind::NonType:
        {
            const auto& P = I_->get<NonTypeTParam>();
            if(! P.Default)
                return nullptr;
            return *P.Default;
        }
        case TParamKind::Template:
        {
            const auto& P = I_->get<TemplateTParam>();
            if(! P.Default)
                return nullptr;
            return *P.Default;
        }
        default:
            MRDOX_UNREACHABLE();
        }
    }
    return nullptr;
}

auto
DomTParam::
props() const ->
    std::vector<std::string_view>
{
    return {
        "kind",
        "name",
        "is-pack",
        "default",
        // only for NonTypeTParam
        "type",
        // only for TemplateTParam
        "params"
    };
}

dom::Value
DomTParamArray::
get(std::size_t index) const
{
    if(index >= list_.size())
        return nullptr;
    return dom::create<DomTParam>(
        list_[index], corpus_);
}

// ----------------------------------------------------------------

dom::Value
DomTArg::
get(std::string_view key) const
{
    if(key == "value")
        return dom::nonEmptyString(I_->Value);
    return nullptr;
}

auto
DomTArg::
props() const ->
    std::vector<std::string_view>
{
    return {
        "value"
    };
}

dom::Value
DomTArgArray::
get(std::size_t index) const
{
    if(index >= list_.size())
        return nullptr;
    return dom::create<DomTArg>(
        list_[index], corpus_);
}

// ----------------------------------------------------------------

DomTemplate::
DomTemplate(
    TemplateInfo const& I,
    Corpus const& corpus) noexcept
    : I_(&I)
    , corpus_(corpus)
{
    if(I_->Primary)
        Primary_ = corpus_.find(*I_->Primary);
    else
        Primary_ = nullptr;
}

dom::Value
DomTemplate::
get(std::string_view key) const
{
    if(key == "kind")
    {
        switch(I_->specializationKind())
        {
        case TemplateSpecKind::Primary:
            return "primary";
        case TemplateSpecKind::Explicit:
            return "explicit";
        case TemplateSpecKind::Partial:
            return "partial";
        default:
            MRDOX_UNREACHABLE();
        }
    }
    if(key == "primary")
    {
        if(! Primary_)
            return nullptr;
        return visit(
            *Primary_,
            [&]<class T>(T const& I) ->
                dom::ObjectPtr
            {
                return dom::create<
                    DomSymbol<T>>(I, corpus_);
            });
    }
    if(key == "params")
        return dom::create<DomTParamArray>(
            I_->Params, corpus_);
    if(key == "args")
        return dom::create<DomTArgArray>(
            I_->Args, corpus_);
    return nullptr;
}

auto
DomTemplate::
props() const ->
    std::vector<std::string_view>
{
    return {
        "params",
        "args",
        "kind",
        "primary"
    };
}

} // mrdox
} // clang
