//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "HandlebarsGenerator.hpp"
#include "HandlebarsCorpus.hpp"
#include "Builder.hpp"
#include "MultiPageVisitor.hpp"
#include "SinglePageVisitor.hpp"
#include <mrdocs/Support/Path.hpp>
#include <sstream>

namespace clang {
namespace mrdocs {
namespace hbs {

std::function<void(OutputRef&, std::string_view)>
createEscapeFn(HandlebarsGenerator const& gen)
{
    return [&gen](OutputRef out, std::string_view str) {
        return gen.escape(out, str);
    };
}

Expected<ExecutorGroup<Builder>>
createExecutors(
    HandlebarsGenerator const& gen,
    HandlebarsCorpus const& hbsCorpus)
{
    auto const& config = hbsCorpus->config;
    auto& threadPool = config.threadPool();
    ExecutorGroup<Builder> group(threadPool);
    for(auto i = threadPool.getThreadCount(); i--;)
    {
        try
        {
           group.emplace(hbsCorpus, createEscapeFn(gen));
        }
        catch(Exception const& ex)
        {
            return Unexpected(ex.error());
        }
    }
    return group;
}

HandlebarsCorpus
createDomCorpus(
    HandlebarsGenerator const& gen,
    Corpus const& corpus)
{
    return {
        corpus,
        gen.fileExtension(),
        [&gen](HandlebarsCorpus const& c, doc::Node const& n) {
            return gen.toString(c, n);
        }};
}

//------------------------------------------------
//
// HandlebarsGenerator
//
//------------------------------------------------

Expected<void>
HandlebarsGenerator::
build(
    std::string_view outputPath,
    Corpus const& corpus) const
{
    if (!corpus.config->multipage)
    {
        return Generator::build(outputPath, corpus);
    }

    // Create corpus and executors
    HandlebarsCorpus domCorpus = createDomCorpus(*this, corpus);
    MRDOCS_TRY(ExecutorGroup<Builder> ex, createExecutors(*this, domCorpus));

    // Visit the corpus
    MultiPageVisitor visitor(ex, outputPath, corpus);
    visitor(corpus.globalNamespace());

    // Wait for all executors to finish and check errors
    auto errors = ex.wait();
    MRDOCS_CHECK_OR(errors.empty(), Unexpected(errors));
    return {};
}

Expected<void>
HandlebarsGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus) const
{
    // Create corpus and executors
    HandlebarsCorpus domCorpus = createDomCorpus(*this, corpus);
    MRDOCS_TRY(ExecutorGroup<Builder> ex, createExecutors(*this, domCorpus));

    // Embedded mode
    if (corpus.config->embedded)
    {
        // Visit the corpus
        SinglePageVisitor visitor(ex, corpus, os);
        visitor(corpus.globalNamespace());

        // Wait for all executors to finish and check errors
        auto errors = ex.wait();
        MRDOCS_CHECK_OR(errors.empty(), Unexpected(errors));

        return {};
    }

    // Wrapped mode
    Builder inlineBuilder(domCorpus, createEscapeFn(*this));
    return inlineBuilder.renderWrapped(os, [&]() -> Expected<void> {
        // This helper will write contents directly to ostream
        SinglePageVisitor visitor(ex, corpus, os);
        visitor(corpus.globalNamespace());

        // Wait for all executors to finish and check errors
        auto errors = ex.wait();
        MRDOCS_CHECK_OR(errors.empty(), Unexpected(errors));

        return {};
    });
}

void
HandlebarsGenerator::
escape(OutputRef& out, std::string_view str) const
{
    out << str;
}

} // hbs
} // mrdocs
} // clang
