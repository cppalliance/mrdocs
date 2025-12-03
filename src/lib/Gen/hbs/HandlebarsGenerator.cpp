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
#include "Builder.hpp"
#include "HandlebarsCorpus.hpp"
#include "MultiPageVisitor.hpp"
#include "SinglePageVisitor.hpp"
#include "TagfileWriter.hpp"
#include <lib/Support/RawOstream.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <format>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string_view>

namespace mrdocs::hbs {

namespace {
constexpr std::string_view defaultStylesheetName = "mrdocs-default.css";
constexpr std::string_view defaultHighlightStylesheetName = "mrdocs-highlight.css";
constexpr std::string_view highlightJsCdn =
    "https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js";

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
    HandlebarsCorpus domCorpus{corpus, fileExtension()};
    prepareCorpus(domCorpus);
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
    HandlebarsCorpus domCorpus{corpus, fileExtension()};
    prepareCorpus(domCorpus);
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
    HandlebarsCorpus domCorpus{corpus, fileExtension()};
    prepareCorpus(domCorpus);
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

std::string
HandlebarsGenerator::
defaultStylesheetSource(Config const& config) const
{
    auto const htmlPath = files::appendPath(
        config->addons,
        "generator",
        "html",
        "layouts",
        "style.css");
    if (files::exists(htmlPath))
    {
        return htmlPath;
    }

    auto const commonPath = files::appendPath(
        config->addons,
        "generator",
        "common",
        "layouts",
        "style.css");
    if (files::exists(commonPath))
    {
        return commonPath;
    }

    return {};
}

std::string
HandlebarsGenerator::
defaultStylesheetOutput(Config const& config) const
{
    return files::appendPath(config->stylesdir, defaultStylesheetName);
}

std::string
HandlebarsGenerator::
defaultHighlightStylesheetSource(Config const& config) const
{
    auto const commonPath = files::appendPath(
        config->addons,
        "generator",
        "common",
        "layouts",
        "highlight.css");
    if (files::exists(commonPath))
    {
        return commonPath;
    }
    return {};
}

std::string
HandlebarsGenerator::
defaultHighlightStylesheetOutput(Config const& config) const
{
    return files::appendPath(config->stylesdir, defaultHighlightStylesheetName);
}

std::string
HandlebarsGenerator::
defaultHighlightScript() const
{
    return std::format(
        R"(// Load highlight.js from CDN and apply to all code blocks
(function() {{
    if (document.querySelector('script[data-mrdocs-hljs]'))
        return;
    var script = document.createElement('script');
    script.src = '{}';
    script.async = true;
    script.setAttribute('data-mrdocs-hljs', 'true');
    script.onload = function() {{
        var run = function() {{
            if (window.hljs)
                hljs.highlightAll();
        }};
        if (document.readyState === 'loading')
            document.addEventListener('DOMContentLoaded', run, {{ once: true }});
        else
            run();
    }};
    document.head.appendChild(script);
}})();)",
        highlightJsCdn);
}

static bool
isRemote(std::string_view path)
{
    return path.starts_with("http://") || path.starts_with("https://");
}

Expected<HandlebarsGenerator::StylesData>
HandlebarsGenerator::
prepareStylesheets(Config const& config) const
{
    StylesData data;

    bool const linkMode = config->linkcss;
    bool const copyCss = config->copycss;

    auto addInlineFromFile = [&](std::string const& path) -> Expected<void>
    {
        MRDOCS_TRY(auto css, files::getFileText(path));
        data.inlineStyles.emplace_back(std::move(css));
        return {};
    };

    auto addLocalLink = [&](std::string const& sourcePath,
                            std::string const& relPath)
    {
        StylesheetRef sheet;
        sheet.sourcePath = sourcePath;
        sheet.outputRelative = files::appendPath(config->stylesdir, relPath);
        sheet.external = false;
        data.stylesheets.push_back(std::move(sheet));
    };

    std::vector<std::string> entries = config->stylesheets;
    if (entries.empty() && !config->noDefaultStyles)
    {
        data.hasDefaultStyles = true;
        auto source = defaultStylesheetSource(config);
        auto output = defaultStylesheetOutput(config);
        if (!source.empty() && !output.empty())
        {
            if (linkMode)
            {
                StylesheetRef sheet;
                sheet.sourcePath = source;
                sheet.outputRelative = output;
                data.stylesheets.push_back(std::move(sheet));
            }
            else
            {
                auto res = addInlineFromFile(source);
                if (!res)
                {
                    report::warn("Failed to read default stylesheet: {}", res.error());
                }
            }
        }

        auto highlightSource = defaultHighlightStylesheetSource(config);
        auto highlightOutput = defaultHighlightStylesheetOutput(config);
        if (!highlightSource.empty() && !highlightOutput.empty())
        {
            if (linkMode)
            {
                addLocalLink(highlightSource, highlightOutput);
            }
            else
            {
                auto res = addInlineFromFile(highlightSource);
                if (!res)
                {
                    report::warn("Failed to read highlight stylesheet: {}", res.error());
                }
            }
        }

        data.inlineScripts.push_back(defaultHighlightScript());
    }

    auto const baseDir = config->configDir();

    for (auto const& entry : entries)
    {
        if (entry.empty())
            continue;

        if (isRemote(entry))
        {
            if (!linkMode)
            {
                return Unexpected(
                    formatError("Remote stylesheet \"{}\" requires linkcss=true", entry));
            }
            StylesheetRef sheet;
            sheet.outputRelative = entry;
            sheet.external = true;
            data.stylesheets.push_back(std::move(sheet));
            continue;
        }

        std::string sourcePath = entry;
        if (!files::isAbsolute(sourcePath))
        {
            sourcePath = files::makeAbsolute(sourcePath, baseDir);
        }
        sourcePath = files::makePosixStyle(sourcePath);

        MRDOCS_CHECK(
            files::exists(sourcePath),
            formatError("Stylesheet path does not exist: {}", sourcePath));

        if (linkMode)
        {
            std::string rel = files::isAbsolute(entry)
                ? std::string(files::getFileName(entry))
                : files::makePosixStyle(entry);
            addLocalLink(sourcePath, rel);
        }
        else
        {
            MRDOCS_TRY(addInlineFromFile(sourcePath));
        }
    }

    for (auto& sheet : data.stylesheets)
    {
        if (!sheet.external)
        {
            sheet.outputRelative = files::makePosixStyle(sheet.outputRelative);
        }
    }

    if (linkMode && copyCss)
    {
        for (auto const& sheet : data.stylesheets)
        {
            if (sheet.external)
                continue;
            auto const targetPath =
                files::appendPath(config->outputDir(), sheet.outputRelative);
            MRDOCS_TRY(files::createDirectory(files::getParentDir(targetPath)));
            std::error_code ec;
            std::filesystem::copy_file(
                sheet.sourcePath,
                targetPath,
                std::filesystem::copy_options::overwrite_existing,
                ec);
            MRDOCS_CHECK(
                !ec,
                formatError(
                    "Failed to copy stylesheet \"{}\" to \"{}\": {}",
                    sheet.sourcePath,
                    targetPath,
                    ec.message()));
        }
    }

    return data;
}

void
HandlebarsGenerator::
prepareCorpus(HandlebarsCorpus& domCorpus) const
{
    if (auto res = prepareStylesheets(domCorpus.getCorpus().config); res)
    {
        auto const& data = *res;
        domCorpus.stylesheets = [&]() {
            dom::Array arr;
            for (auto const& sheet : data.stylesheets)
            {
                dom::Object obj;
                obj.set("path", sheet.outputRelative);
                obj.set("external", sheet.external);
                arr.emplace_back(std::move(obj));
            }
            return arr;
        }();

        dom::Array inlineArr;
        for (auto const& css : data.inlineStyles)
            inlineArr.emplace_back(css);
        domCorpus.inlineStyles = inlineArr;

        dom::Array inlineScriptArr;
        for (auto const& script : data.inlineScripts)
            inlineScriptArr.emplace_back(script);
        domCorpus.inlineScripts = inlineScriptArr;
        domCorpus.hasDefaultStyles = data.hasDefaultStyles;
    }
    else
    {
        report::warn(
            "Failed to prepare stylesheets for corpus: {}",
            res.error());
    }
}

} // mrdocs::hbs
