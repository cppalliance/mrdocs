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

#include "lib/Lib/TagfileWriter.hpp"
#include "lib/Support/RawOstream.hpp"

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>

#include <fstream>
#include <sstream>

namespace clang::mrdocs::hbs {

namespace {
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
    Corpus const& corpus) {
    return {
        corpus,
        gen.fileExtension(),
        [&gen](HandlebarsCorpus const& c, doc::Node const& n) {
            return gen.toString(c, n);
        }};
}
} // (anon)

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
        MRDOCS_TRY(Generator::build(outputPath, corpus));
        MRDOCS_CHECK_OR(!corpus.config->tagfile.empty(), {});
        MRDOCS_TRY(buildTagfile(corpus.config->tagfile, corpus));
        return {};
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
    report::info("Generated {} pages", visitor.count());

    MRDOCS_CHECK_OR(!corpus.config->tagfile.empty(), {});
    MRDOCS_TRY(buildTagfile(corpus.config->tagfile, corpus));
    return {};
}

Expected<void>
HandlebarsGenerator::
buildTagfile(
    std::ostream& os,
    Corpus const& corpus) const
{
    HandlebarsCorpus domCorpus = createDomCorpus(*this, corpus);
    RawOstream raw_os(os);
    if (corpus.config->multipage)
    {
        MRDOCS_TRY(auto tagFileWriter, TagfileWriter::create(
                        domCorpus,
                        raw_os));
        tagFileWriter.build();
    }
    else
    {
        // Get the name of the single page output file
        auto const singlePagePath = getSinglePageFullPath(corpus.config->output, fileExtension());
        MRDOCS_CHECK_OR(singlePagePath, Unexpected(singlePagePath.error()));
        auto const singlePathFilename = files::getFileName(*singlePagePath);
        MRDOCS_TRY(auto tagFileWriter, TagfileWriter::create(
                                domCorpus,
                                raw_os,
                                singlePathFilename));
        tagFileWriter.build();
    }
    return {};
}

Expected<void>
HandlebarsGenerator::
buildTagfile(
    std::string_view const fileName,
    Corpus const& corpus) const
{
    std::string const dir = files::getParentDir(fileName);
    MRDOCS_TRY(files::createDirectory(dir));
    std::ofstream os;
    try
    {
        os.open(std::string(fileName),
            std::ios_base::binary |
                std::ios_base::out |
                std::ios_base::trunc // | std::ios_base::noreplace
            );
    }
    catch(std::exception const& ex)
    {
        return Unexpected(formatError("std::ofstream threw \"{}\"", ex.what()));
    }
    try
    {
        return buildTagfile(os, corpus);
    }
    catch(std::exception const& ex)
    {
        return Unexpected(formatError("buildOne threw \"{}\"", ex.what()));
    }
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

} // clang::mrdocs::hbs