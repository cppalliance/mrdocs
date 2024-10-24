//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "AdocCorpus.hpp"
#include "AdocGenerator.hpp"
#include "Builder.hpp"
#include "MultiPageVisitor.hpp"
#include "SinglePageVisitor.hpp"
#include "lib/Support/LegibleNames.hpp"
#include "lib/Support/RawOstream.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <mrdocs/Metadata/DomMetadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <fstream>
#include <optional>
#include <vector>

namespace clang {
namespace mrdocs {
namespace adoc {

Expected<ExecutorGroup<Builder>>
createExecutors(
    AdocCorpus const& adocCorpus)
{
    auto const& config = adocCorpus->config;
    auto& threadPool = config.threadPool();
    ExecutorGroup<Builder> group(threadPool);
    for(auto i = threadPool.getThreadCount(); i--;)
    {
        try
        {
           group.emplace(adocCorpus);
        }
        catch(Exception const& ex)
        {
            return Unexpected(ex.error());
        }
    }
    return group;
}

//------------------------------------------------
//
// AdocGenerator
//
//------------------------------------------------

Error
AdocGenerator::
build(
    std::string_view outputPath,
    Corpus const& corpus) const
{
    if(! corpus.config->multipage)
        return Generator::build(outputPath, corpus);

    auto options = loadOptions(corpus);
    if(! options)
        return options.error();

    AdocCorpus domCorpus(corpus, *std::move(options));
    auto ex = createExecutors(domCorpus);
    if(! ex)
        return ex.error();

    std::string path = files::appendPath(outputPath, "reference.tag.xml");
    if(auto err = files::createDirectory(outputPath)) 
    {
        return err;
    }

    std::ofstream os;
    try
    {
        os.open(path,
            std::ios_base::binary |
                std::ios_base::out |
                std::ios_base::trunc // | std::ios_base::noreplace
            );
    }
    catch(std::exception const& ex)
    {
        return formatError("std::ofstream(\"{}\") threw \"{}\"", path, ex.what());
    }    

    RawOstream raw_os(os);
    auto tagfileWriter = TagfileWriter(raw_os, corpus);
    tagfileWriter.initialize();

    MultiPageVisitor visitor(*ex, outputPath, corpus, tagfileWriter);
    visitor(corpus.globalNamespace());

    auto errors = ex->wait();
    tagfileWriter.finalize();
    if(! errors.empty())
        return Error(errors);
    return Error::success();
}

Error
AdocGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus,
    std::string_view outputPath) const
{
    namespace path = llvm::sys::path;
    using SmallString = llvm::SmallString<0>;

    auto options = loadOptions(corpus);
    if(! options)
        return options.error();

    AdocCorpus domCorpus(corpus, *std::move(options));
    auto ex = createExecutors(domCorpus);
    if(! ex)
        return ex.error();

    std::vector<Error> errors;

    ex->async(
        [&os](Builder& builder)
        {
            auto pageText = builder.renderSinglePageHeader().value();
            os.write(pageText.data(), pageText.size());
        });
    errors = ex->wait();
    if(! errors.empty())
        return {errors};

    SmallString fileName(outputPath);
    path::replace_extension(fileName, "tag.xml");
    auto parentDir = files::getParentDir(fileName.str());
    if(auto err = files::createDirectory(parentDir)) 
    {
        return err;
    }

    std::ofstream osTagfile;
    try
    {
        osTagfile.open(fileName.str().str(),
            std::ios_base::binary |
                std::ios_base::out |
                std::ios_base::trunc // | std::ios_base::noreplace
            );
    }
    catch(std::exception const& ex)
    {
        return formatError("std::ofstream(\"{}\") threw \"{}\"", fileName.str().str(), ex.what());
    }    

    RawOstream raw_os(osTagfile);
    auto tagfileWriter = TagfileWriter(raw_os, corpus);
    tagfileWriter.initialize();


    auto const justFileName = path::filename(fileName);
    SinglePageVisitor visitor(*ex, corpus, os, justFileName, tagfileWriter);
    visitor(corpus.globalNamespace());
    errors = ex->wait();
    tagfileWriter.finalize();
    if(! errors.empty())
        return {errors};

    ex->async(
        [&os](Builder& builder)
        {
            auto pageText = builder.renderSinglePageFooter().value();
            os.write(pageText.data(), pageText.size());
        });
    errors = ex->wait();
    if(! errors.empty())
        return {errors};

    return Error::success();
}

} // adoc

//------------------------------------------------

std::unique_ptr<Generator>
makeAdocGenerator()
{
    return std::make_unique<adoc::AdocGenerator>();
}

} // mrdocs
} // clang
