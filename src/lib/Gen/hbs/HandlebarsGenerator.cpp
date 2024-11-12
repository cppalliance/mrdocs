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
#include "Options.hpp"
#include "MultiPageVisitor.hpp"
#include "SinglePageVisitor.hpp"
#include <mrdocs/Support/Path.hpp>

namespace clang {
namespace mrdocs {
namespace hbs {

Expected<ExecutorGroup<Builder>>
createExecutors(
    HandlebarsCorpus const& hbsCorpus)
{
    auto const& config = hbsCorpus->config;
    auto& threadPool = config.threadPool();
    ExecutorGroup<Builder> group(threadPool);
    for(auto i = threadPool.getThreadCount(); i--;)
    {
        try
        {
           group.emplace(hbsCorpus);
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
// HandlebarsGenerator
//
//------------------------------------------------

/** Return loaded Options from a configuration.
*/
Options
loadOptions(
    std::string_view fileExtension,
    Config const& config)
{
    Options opt;
    dom::Value domOpts = config.object().get("generator").get(fileExtension);
    if (domOpts.get("legible-names").isBoolean())
    {
        opt.legible_names = domOpts.get("legible-names").getBool();
    }
    if (domOpts.get("template-dir").isString())
    {
        opt.template_dir = domOpts.get("template-dir").getString();
        opt.template_dir = files::makeAbsolute(
            opt.template_dir,
            config->configDir);
    }
    else
    {
        opt.template_dir = files::appendPath(
            config->addons,
            "generator",
            fileExtension);
    }
    return opt;
}

Error
HandlebarsGenerator::
build(
    std::string_view outputPath,
    Corpus const& corpus) const
{
    if (!corpus.config->multipage)
    {
        return Generator::build(outputPath, corpus);
    }

    Options options = loadOptions(fileExtension(), corpus.config);
    HandlebarsCorpus domCorpus(
        corpus, std::move(options), fileExtension(), toStringFn);
    auto ex = createExecutors(domCorpus);
    if (!ex)
    {
        return ex.error();
    }

    MultiPageVisitor visitor(*ex, outputPath, corpus);
    visitor(corpus.globalNamespace());

    auto errors = ex->wait();
    if (!errors.empty())
    {
        return Error(errors);
    }
    return Error::success();
}

Error
HandlebarsGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus) const
{
    auto options = loadOptions(fileExtension(), corpus.config);

    HandlebarsCorpus domCorpus(
        corpus, std::move(options), fileExtension(), toStringFn);
    auto ex = createExecutors(domCorpus);
    if (!ex)
    {
        return ex.error();
    }

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

    SinglePageVisitor visitor(*ex, corpus, os);
    visitor(corpus.globalNamespace());
    errors = ex->wait();
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

} // hbs
} // mrdocs
} // clang
