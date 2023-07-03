//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "AdocCorpus.hpp"
#include "AdocGenerator.hpp"
#include "Builder.hpp"
#include "MultiPageVisitor.hpp"
#include "SinglePageVisitor.hpp"
#include "Support/SafeNames.hpp"
#include <mrdox/Metadata/DomMetadata.hpp>
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/Report.hpp>
#include <optional>
#include <vector>

namespace clang {
namespace mrdox {
namespace adoc {

Expected<ExecutorGroup<Builder>>
createExecutors(
    DomCorpus const& domCorpus)
{
    auto options = loadOptions(domCorpus.corpus);
    if(! options)
        return options.error();

    auto const& config = domCorpus.corpus.config;
    auto& threadPool = config.threadPool();
    ExecutorGroup<Builder> group(threadPool);
    for(auto i = threadPool.getThreadCount(); i--;)
    {
        try
        {
           group.emplace(domCorpus, *options);
        }
        catch(Error const& e)
        {
            return e;
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
    if(! corpus.config.multiPage)
        return Generator::build(outputPath, corpus);

    AdocCorpus domCorpus(corpus);
    auto ex = createExecutors(domCorpus);
    if(! ex)
        return ex.error();

    MultiPageVisitor visitor(*ex, outputPath, corpus);
    visitor(corpus.globalNamespace());
    auto errors = ex->wait();
    if(! errors.empty())
        return Error(errors);
    return Error::success();
}

Error
AdocGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus) const
{
    AdocCorpus domCorpus(corpus);
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
        return Error(errors);

    SinglePageVisitor visitor(*ex, corpus, os);
    visitor(corpus.globalNamespace());
    errors = ex->wait();
    if(! errors.empty())
        return Error(errors);

    ex->async(
        [ &os](Builder& builder)
        {
            auto pageText = builder.renderSinglePageFooter().value();
            os.write(pageText.data(), pageText.size());
        });
    errors = ex->wait();
    if(! errors.empty())
        return Error(errors);

    return Error::success();
}

} // adoc

//------------------------------------------------

std::unique_ptr<Generator>
makeAdocGenerator()
{
    return std::make_unique<adoc::AdocGenerator>();
}

} // mrdox
} // clang
