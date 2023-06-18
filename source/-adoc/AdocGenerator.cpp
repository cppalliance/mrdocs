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

#include "AdocGenerator.hpp"
#include "Builder.hpp"
#include "MultiPageVisitor.hpp"
#include "SinglePageVisitor.hpp"
#include "Support/SafeNames.hpp"
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/Report.hpp>
#include <optional>
#include <vector>

namespace clang {
namespace mrdox {
namespace adoc {

Expected<ExecutorGroup<Builder>>
createExecutors(
    Corpus const& corpus)
{
    auto options = loadOptions(corpus);
    if(! options)
        return options.getError();

    auto const& config = corpus.config;
    auto& threadPool = config.threadPool();
    ExecutorGroup<Builder> group(threadPool);
    for(auto i = threadPool.getThreadCount(); i--;)
    {
        try
        {
           group.emplace(corpus, *options);
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
    if(corpus.config.singlePage)
        return Generator::build(outputPath, corpus);

    auto ex = createExecutors(corpus);
    if(! ex)
        return ex.getError();

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
    auto ex = createExecutors(corpus);
    if(! ex)
        return ex.getError();

    SinglePageVisitor visitor(*ex, corpus, os);
    visitor(corpus.globalNamespace());
    auto errors = ex->wait();
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
