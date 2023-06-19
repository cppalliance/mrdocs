//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMTEMPLATE_HPP
#define MRDOX_API_DOM_DOMTEMPLATE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>

namespace clang {
namespace mrdox {

class MRDOX_DECL
    DomTParam : public dom::Object
{
    TParam const* I_;
    Corpus const& corpus_;

public:
    DomTParam(
        TParam const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
    }

    dom::Value get(std::string_view key) const override;
    std::vector<std::string_view> props() const override;
};

// ----------------------------------------------------------------

class MRDOX_DECL
    DomTArg : public dom::Object
{
    TArg const* I_;
    Corpus const& corpus_;

public:
    DomTArg(
        TArg const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
    }

    dom::Value get(std::string_view key) const override;
    std::vector<std::string_view> props() const override;
};

// ----------------------------------------------------------------

/** An array of template parameters
*/
class DomTParamArray : public dom::Array
{
    std::vector<TParam> const& list_;
    Corpus const& corpus_;

public:
    DomTParamArray(
        std::vector<TParam> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override;
};

// ----------------------------------------------------------------

/** An array of template arguments
*/
class DomTArgArray : public dom::Array
{
    std::vector<TArg> const& list_;
    Corpus const& corpus_;

public:
    DomTArgArray(
        std::vector<TArg> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override;
};

// ----------------------------------------------------------------

/** Template info
*/
class MRDOX_DECL
    DomTemplate : public dom::Object
{
    TemplateInfo const* I_;
    Info const* Primary_;
    Corpus const& corpus_;

public:
    DomTemplate(
        TemplateInfo const& I,
        Corpus const& corpus) noexcept;

    dom::Value get(std::string_view key) const override;
    std::vector<std::string_view> props() const override;
};

} // mrdox
} // clang

#endif
