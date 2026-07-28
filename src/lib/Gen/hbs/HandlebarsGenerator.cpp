//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "HandlebarsGenerator.hpp"
#include "AddonPaths.hpp"
#include "Builder.hpp"
#include "HandlebarsCorpus.hpp"
#include "MultiPageVisitor.hpp"
#include "SinglePageVisitor.hpp"
#include "TagfileWriter.hpp"
#include <lib/Support/Generator.hpp>
#include <lib/Support/RawOstream.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

namespace mrdocs::hbs {

namespace {

/// Default filename for the main MrDocs stylesheet.
constexpr std::string_view defaultStylesheetName = "mrdocs-default.css";

/// Default filename for the syntax highlighting stylesheet.
constexpr std::string_view defaultHighlightStylesheetName = "mrdocs-highlight.css";

/// CDN URL for highlight.js library used for syntax highlighting.
constexpr std::string_view highlightJsCdn =
    "https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js";

/** Creates an escape function bound to a generator.

    Returns a callable that delegates to the generator's escape method.
    Used by Handlebars to escape output strings according to the
    output format (e.g., HTML escaping for HTML output).

    @param gen The generator providing the escape implementation.
    @return A function suitable for use as a Handlebars escape function.
*/
std::function<void(OutputRef&, std::string_view)>
createEscapeFn(HandlebarsGenerator const& gen)
{
    return [&gen](OutputRef out, std::string_view str) {
        return gen.escape(out, str);
    };
}

/** Creates an executor group with Builder instances for parallel rendering.

    Initializes one Builder per thread in the thread pool. Each Builder
    has its own Handlebars instance with registered templates, partials,
    and helpers, enabling lock-free parallel page generation.

    @param gen The generator providing escape function configuration.
    @param hbsCorpus The corpus containing symbol data and configuration.
    @return An executor group ready for parallel rendering, or an error
            if Builder initialization fails.
*/
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

HandlebarsGenerator::
HandlebarsGenerator(
    std::string const& id,
    std::string const& fileExtension,
    std::string const& displayName,
    EscapeMap escapeMap,
    std::string extends)
    : id_(id)
    , fileExtension_(fileExtension)
    , displayName_(displayName)
    , extends_(std::move(extends))
    , escapeMap_(std::move(escapeMap))
{
}

/** Whether the generator renders a tree of pages rather than a single page.

    @param config The configuration to read the flag from.
    @param generatorId The id of the generator being rendered.
*/
bool
rendersMultiPage(Config const& config, std::string_view generatorId)
{
    auto const& genOpts = config->generatorOptions;
    if (auto const it = genOpts.find(std::string(generatorId));
        it != genOpts.end())
    {
        if (dom::Value const m = it->second.get("multipage"); m.isBoolean())
        {
            return m.getBool();
        }
    }
    return config->multipage;
}

Expected<void>
HandlebarsGenerator::
build(Corpus const& corpus) const
{
    // Resolve where this generator writes (a directory for a multi-page
    // render, a file or directory for a single document) from the config.
    std::string const outputPath = getGeneratorOutputPath(*this, corpus);
    std::string_view const configuredTagfile = corpus.config->tagfile;

    // The tagfile and the copied stylesheets land in the same directory as
    // the generator's output, so they never collide between generators.
    auto writeTagfile = [&](std::string_view dir) -> Expected<void>
    {
        MRDOCS_CHECK_OR(!configuredTagfile.empty(), {});
        std::string const tagfilePath = files::appendPath(
            dir, files::getFileName(configuredTagfile));
        return buildTagfile(tagfilePath, corpus);
    };

    if (!rendersMultiPage(corpus.config, id()))
    {
        // Single document: write it to the resolved file (a directory
        // output becomes reference.<ext>), then the stylesheets and tagfile
        // beside it.
        MRDOCS_TRY(
            std::string const file,
            getSinglePageFullPath(outputPath, fileExtension()));
        std::string const outDir(files::getParentDir(file));
        MRDOCS_TRY(writeToFile(file, [&](std::ostream& os) -> Expected<void>
        {
            return renderSinglePage(os, corpus);
        }));
        MRDOCS_TRY(copyStylesheets(corpus.config, outDir));
        MRDOCS_TRY(writeTagfile(outDir));
        return {};
    }

    // Multi-page: one file per symbol inside the output directory.
    HandlebarsCorpus domCorpus{corpus, fileExtension()};
    prepareCorpus(domCorpus);
    MRDOCS_TRY(ExecutorGroup<Builder> ex, createExecutors(*this, domCorpus));
    MultiPageVisitor visitor(ex, outputPath, corpus);
    visitor(corpus.globalNamespace());

    auto errors = ex.wait();
    MRDOCS_CHECK_OR(errors.empty(), Unexpected(errors));
    report::info("Generated {} pages", visitor.count());

    MRDOCS_TRY(copyStylesheets(corpus.config, outputPath));
    MRDOCS_TRY(writeTagfile(outputPath));
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
    if (rendersMultiPage(corpus.config, id()))
    {
        MRDOCS_TRY(auto tagFileWriter, TagfileWriter::create(
                        domCorpus,
                        raw_os));
        tagFileWriter.build();
    }
    else
    {
        // The single-page filename is the same regardless of which
        // directory this generator writes to.
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
    std::string const dir(files::getParentDir(fileName));
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
        return Unexpected(formatError("buildTagfile threw \"{}\"", ex.what()));
    }
}

Expected<void>
HandlebarsGenerator::
renderSinglePage(
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
EscapeMap::
set(std::string_view source, std::string_view replacement)
{
    if (source.size() == 1)
    {
        set(source[0], replacement);
        return;
    }
    auto& bucket = multiByte_[static_cast<unsigned char>(source[0])];
    // Update in place when the same source is registered twice.
    for (auto& entry : bucket)
    {
        if (entry.first == source)
        {
            entry.second.assign(replacement);
            return;
        }
    }
    bucket.emplace_back(std::string(source), std::string(replacement));
}

void
EscapeMap::
apply(OutputRef& out, std::string_view str) const
{
    std::size_t i = 0;
    while (i < str.size())
    {
        auto const byte = static_cast<unsigned char>(str[i]);
        // Multi-byte path: only entered when this byte has at least
        // one multi-byte rule registered. The longest match wins, so
        // a `**` rule takes precedence over a `*` rule at the same
        // position.
        auto const& bucket = multiByte_[byte];
        if (!bucket.empty())
        {
            std::string const* longestRepl = nullptr;
            std::size_t longestLen = 0;
            std::size_t const remaining = str.size() - i;
            for (auto const& [pattern, repl] : bucket)
            {
                if (pattern.size() <= remaining &&
                    pattern.size() > longestLen &&
                    str.compare(i, pattern.size(), pattern) == 0)
                {
                    longestRepl = &repl;
                    longestLen = pattern.size();
                }
            }
            if (longestRepl)
            {
                out << *longestRepl;
                i += longestLen;
                continue;
            }
        }
        // Single-byte fallback: array lookup, no allocation.
        std::string const& r = singleByte_[byte];
        if (r.empty())
        {
            out << str[i];
        }
        else
        {
            out << r;
        }
        ++i;
    }
}

void
HandlebarsGenerator::
escape(OutputRef& out, std::string_view str) const
{
    escapeMap_.apply(out, str);
}

std::string
HandlebarsGenerator::
defaultStylesheetSource(Config const& config) const
{
    if (auto path = addon_paths::findFile(config, "html", "layouts", "style.css"))
        return *path;
    if (auto path = addon_paths::findFile(config, "common", "layouts", "style.css"))
        return *path;
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
    if (auto path = addon_paths::findFile(config, "common", "layouts", "highlight.css"))
        return *path;
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

/** Checks if a path is a remote URL.

    @param path The path or URL to check.
    @return True if the path starts with "http://" or "https://".
*/
static bool
isRemote(std::string_view path)
{
    return path.starts_with("http://") || path.starts_with("https://");
}

/*  Collect the stylesheets and scripts a rendered page references.

    This produces data only; copyStylesheets() writes any files. Each
    stylesheet is either linked (recorded so its file can be copied next to
    the output) or inlined (its contents read into inlineStyles for a
    <style> block), depending on the `linkcss` option.

    With no user `stylesheets` and default styles enabled, the built-in main
    and highlight stylesheets and the highlight.js loader script are added.
    Otherwise each `stylesheets` entry is used: a remote URL becomes an
    external link (link mode only); a local path is resolved against the
    config directory and linked or inlined.
*/
Expected<HandlebarsGenerator::StylesData>
HandlebarsGenerator::
prepareStylesheets(Config const& config) const
{
    StylesData data;
    bool const linkMode = config->linkcss;

    auto addInlineFromFile = [&](std::string const& path) -> Expected<void>
    {
        MRDOCS_TRY(auto css, files::getFileText(path));
        data.inlineStyles.emplace_back(std::move(css));
        return {};
    };

    // Record a local stylesheet: linked (its file is copied to
    // `outputRelative` later) or inlined into the page.
    auto linkOrInline = [&](std::string const& sourcePath,
                            std::string const& outputRelative) -> Expected<void>
    {
        if (!linkMode)
        {
            return addInlineFromFile(sourcePath);
        }
        data.stylesheets.push_back(
            StylesheetRef{sourcePath, outputRelative, false});
        return {};
    };

    std::vector<std::string> const entries = config->stylesheets;
    if (entries.empty() && !config->noDefaultStyles)
    {
        data.hasDefaultStyles = true;
        auto const source = defaultStylesheetSource(config);
        auto const output = defaultStylesheetOutput(config);
        if (!source.empty() && !output.empty())
        {
            if (auto r = linkOrInline(source, output); !r)
            {
                report::warn("Failed to read default stylesheet: {}", r.error());
            }
        }
        auto const hlSource = defaultHighlightStylesheetSource(config);
        auto const hlOutput = defaultHighlightStylesheetOutput(config);
        if (!hlSource.empty() && !hlOutput.empty())
        {
            auto const rel = files::appendPath(config->stylesdir, hlOutput);
            if (auto r = linkOrInline(hlSource, rel); !r)
            {
                report::warn("Failed to read highlight stylesheet: {}", r.error());
            }
        }
        data.inlineScripts.push_back(defaultHighlightScript());
    }

    auto const baseDir = config->configDir();
    for (auto const& entry : entries)
    {
        if (entry.empty())
        {
            continue;
        }
        if (isRemote(entry))
        {
            MRDOCS_CHECK(linkMode, formatError(
                "Remote stylesheet \"{}\" requires linkcss=true", entry));
            data.stylesheets.push_back(StylesheetRef{"", entry, true});
            continue;
        }
        std::string sourcePath = files::makePosixStyle(
            files::isAbsolute(entry)
                ? entry
                : files::makeAbsolute(entry, baseDir));
        MRDOCS_CHECK(
            files::exists(sourcePath),
            formatError("Stylesheet path does not exist: {}", sourcePath));
        std::string const rel = files::isAbsolute(entry)
            ? std::string(files::getFileName(entry))
            : files::makePosixStyle(entry);
        MRDOCS_TRY(linkOrInline(
            sourcePath, files::appendPath(config->stylesdir, rel)));
    }

    for (auto& sheet : data.stylesheets)
    {
        if (!sheet.external)
        {
            sheet.outputRelative = files::makePosixStyle(sheet.outputRelative);
        }
    }
    return data;
}

Expected<void>
HandlebarsGenerator::
copyStylesheets(Config const& config, std::string_view outputDir) const
{
    // Only linked-and-copied CSS lands on disk, and only when we have a
    // directory to copy it into (the stream/embedded paths do not).
    if (!config->linkcss || !config->copycss || outputDir.empty())
    {
        return {};
    }
    MRDOCS_TRY(StylesData const data, prepareStylesheets(config));
    for (auto const& sheet : data.stylesheets)
    {
        if (sheet.external)
        {
            continue;
        }
        auto const targetPath =
            files::appendPath(outputDir, sheet.outputRelative);
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
    return {};
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
