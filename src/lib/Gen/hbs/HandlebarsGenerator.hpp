//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HBS_HANDLEBARSGENERATOR_HPP
#define MRDOCS_LIB_GEN_HBS_HANDLEBARSGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <lib/Gen/hbs/Options.hpp>
#include <lib/Gen/hbs/HandlebarsCorpus.hpp>
#include <utility>

namespace clang {
namespace mrdocs {
namespace hbs {

class HandlebarsGenerator
    : public Generator
{
    std::string displayName_;
    std::string fileExtension_;
    using JavadocToStringFn = std::function<std::string(HandlebarsCorpus const&, doc::Node const&)>;
    JavadocToStringFn toStringFn;

public:
    HandlebarsGenerator(
        std::string_view displayName,
        std::string_view fileExtension,
        JavadocToStringFn toStringFn)
        : displayName_(displayName)
        , fileExtension_(fileExtension)
        , toStringFn(std::move(toStringFn))
    {}

    std::string_view
    id() const noexcept override
    {
        return fileExtension_;
    }

    std::string_view
    displayName() const noexcept override
    {
        return displayName_;
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return fileExtension_;
    }

    Error
    build(
        std::string_view outputPath,
        Corpus const& corpus) const override;

    Error
    buildOne(
        std::ostream& os,
        Corpus const& corpus) const override;
};

} // hbs
} // mrdocs
} // clang

#endif
