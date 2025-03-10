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

namespace clang::mrdocs {

class OutputRef;

namespace hbs {

class HandlebarsGenerator
    : public Generator
{
public:
    Expected<void>
    build(
        std::string_view outputPath,
        Corpus const& corpus) const override;

    Expected<void>
    buildOne(
        std::ostream& os,
        Corpus const& corpus) const override;

    /** Build a tagfile for the corpus.
     */
    Expected<void>
    buildTagfile(
        std::ostream& os,
        Corpus const& corpus) const;

    /** Build a tagfile for the corpus and store the result in a file.
     */
    Expected<void>
    buildTagfile(
        std::string_view fileName,
        Corpus const& corpus) const;

    /** Output an escaped string to the output stream.
    */
    virtual
    void
    escape(OutputRef& os, std::string_view str) const;
};

} // hbs

} // clang::mrdocs

#endif
