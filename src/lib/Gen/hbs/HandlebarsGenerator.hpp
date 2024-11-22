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
#include <lib/Gen/hbs/HandlebarsCorpus.hpp>
#include <utility>

namespace clang {
namespace mrdocs {

class OutputRef;

namespace hbs {

class HandlebarsGenerator
    : public Generator
{
    std::string displayName_;
    std::string fileExtension_;

public:
    HandlebarsGenerator(
        std::string_view displayName,
        std::string_view fileExtension)
        : displayName_(displayName)
        , fileExtension_(fileExtension)
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

    Expected<void>
    build(
        std::string_view outputPath,
        Corpus const& corpus) const override;

    Expected<void>
    buildOne(
        std::ostream& os,
        Corpus const& corpus) const override;

    /** Convert a Javadoc node to a string.
    */
    virtual
    std::string
    toString(HandlebarsCorpus const&, doc::Node const&) const
    {
        return {};
    }

    /** Output a escaped string to the output stream.
    */
    virtual
    void
    escape(OutputRef& os, std::string_view str) const;
};

} // hbs
} // mrdocs
} // clang

#endif
