//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Builder.hpp"
#include <lib/Lib/ConfigImpl.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/Path.h>
#include <fmt/format.h>
#include <filesystem>

namespace clang {
namespace mrdocs {

namespace lua {
extern void lua_dump(dom::Object const& obj);
}

namespace hbs {

namespace {
void
loadPartials(
    Handlebars& hbs,
    std::string const& partialsPath)
{
    if (!files::exists(partialsPath))
    {
        return;
    }
    auto exp = forEachFile(partialsPath, true,
        [&](std::string_view pathName) -> Expected<void>
        {
            // Skip directories
            MRDOCS_CHECK_OR(!files::isDirectory(pathName), {});

            // Get template relative path
            std::filesystem::path relPath = pathName;
            relPath = relPath.lexically_relative(partialsPath);

            // Skip non-handlebars files
            MRDOCS_CHECK_OR(relPath.extension() == ".hbs", {});

            // Remove any file extensions
            while(relPath.has_extension())
            {
                relPath.replace_extension();
            }

            // Load partial contents
            MRDOCS_TRY(std::string text, files::getFileText(pathName));

            // Register partial
            hbs.registerPartial(relPath.generic_string(), text);
            return {};
        });
    if (!exp)
    {
        exp.error().Throw();
    }
}

/* Make a URL relative to another URL.

   This function is a version of the Antora `relativize` helper,
   used to create relative URLs between two paths in Antora projects.

   The function takes two paths, `to` and `from`, and returns a
   relative path from `from` to `to`.

   If `from` is not provided, then the URL of the symbol being
   rendered is used as the `from` path.

   @see https://gitlab.com/antora/antora-ui-default/-/blob/master/src/helpers/relativize.js
 */
dom::Value
relativize_fn(dom::Value to0, dom::Value from0, dom::Value options)
{
    if (!to0)
    {
        return "#";
    }

    if (!to0.isString())
    {
        return to0;
    }

    std::string_view to = to0.getString().get();
    if (!to.starts_with('/'))
    {
        return to0;
    }

    // Single argument invocation
    bool const singleArg = !options;
    if (singleArg)
    {
        // NOTE only legacy invocation provides both to and from0
        options = from0;
        from0 = options.lookup("data.root.symbol.url");
    }

    // If `from` is still not set as a string
    if (!from0.isString() || from0.getString().empty())
    {
        return to;
    }
    std::string_view const from = from0.getString().get();

    // Find anchor in URL
    std::string_view hash;
    std::size_t const hashIdx = to.find('#');
    if (hashIdx != std::string_view::npos)
    {
        hash = to.substr(hashIdx);
        to = to.substr(0, hashIdx);
    }

    // Handle the case where they are the same URL
    if (to == from)
    {
        if (!hash.empty())
        {
            return hash;
        }
        if (files::isDirsy(to))
        {
            return "./";
        }
        return files::getFileName(to);
    }

    // Handle the general case
    std::string const fromDir = files::getParentDir(from);
    std::string relativePath = std::filesystem::path(to).lexically_relative(fromDir).generic_string();
    if (relativePath.empty())
    {
        relativePath = ".";
    }
    if (!relativePath.starts_with("../") && !relativePath.starts_with("./"))
    {
        // relative hrefs needs to explicitly include "./" so that
        // they are always treated as relative to the current page
        relativePath = "./" + relativePath;
    }
    relativePath += hash;
    return relativePath;
}

} // (anon)



Builder::
Builder(
    HandlebarsCorpus const& corpus,
    std::function<void(OutputRef&, std::string_view)> escapeFn)
    : escapeFn_(std::move(escapeFn))
    , domCorpus(corpus)
{
    namespace fs = std::filesystem;

    // load partials
    loadPartials(hbs_, commonTemplatesDir("partials"));
    loadPartials(hbs_, templatesDir("partials"));

    // Load JavaScript helpers
    std::string helpersPath = templatesDir("helpers");
    auto exp = forEachFile(helpersPath, true,
        [&](std::string_view pathName)-> Expected<void>
        {
            // Register JS helper function in the global object
            constexpr std::string_view ext = ".js";
            if (!pathName.ends_with(ext)) return {};
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            MRDOCS_TRY(auto script, files::getFileText(pathName));
            MRDOCS_TRY(js::registerHelper(hbs_, name, ctx_, script));
            return {};
        });
    if (!exp)
    {
        exp.error().Throw();
    }

    hbs_.registerHelper("primary_location",
        dom::makeInvocable([](dom::Value const& v) ->
            dom::Value
        {
            dom::Value const sourceInfo = v.get("loc");
            if (!sourceInfo)
            {
                return nullptr;
            }
            dom::Value decls = sourceInfo.get("decl");
            if(dom::Value def = sourceInfo.get("def"))
            {
                // for classes/enums, prefer the definition
                if (dom::Value const kind = v.get("kind");
                    kind == "record" || kind == "enum")
                {
                    return def;
                }
                // We only want to use the definition
                // for non-tag types when no other declaration
                // exists
                if (!decls)
                {
                    return def;
                }
            }
            if (!decls.isArray() ||
                 decls.getArray().empty())
            {
                return nullptr;
            }
            // Use whatever declaration had docs.
            for (dom::Value const& loc : decls.getArray())
            {
                if (loc.get("documented"))
                {
                    return loc;
                }
            }
            // if no declaration had docs, fallback to the
            // first declaration
            return decls.getArray().get(0);
        }));
    helpers::registerConstructorHelpers(hbs_);
    helpers::registerStringHelpers(hbs_);
    helpers::registerAntoraHelpers(hbs_);
    helpers::registerLogicalHelpers(hbs_);
    helpers::registerMathHelpers(hbs_);
    helpers::registerContainerHelpers(hbs_);
    hbs_.registerHelper("relativize", dom::makeInvocable(relativize_fn));

    // Load layout templates
    std::string indexTemplateFilename = fmt::format("index.{}.hbs", domCorpus.fileExtension);
    std::string wrapperTemplateFilename = fmt::format("wrapper.{}.hbs", domCorpus.fileExtension);
    for (std::string const& filename : {indexTemplateFilename, wrapperTemplateFilename})
    {
        std::string pathName = files::appendPath(layoutDir(), filename);
        Expected<std::string> text = files::getFileText(pathName);
        if (!text)
        {
            text.error().Throw();
        }
        templates_.emplace(filename, text.value());
    }
}

//------------------------------------------------

Expected<void>
Builder::
callTemplate(
    std::ostream& os,
    std::string_view name,
    dom::Value const& context)
{
    auto it = templates_.find(name);
    MRDOCS_CHECK(it != templates_.end(), formatError("Template {} not found", name));
    std::string_view fileText = it->second;
    HandlebarsOptions options;
    options.escapeFunction = escapeFn_;
    OutputRef out(os);
    Expected<void, HandlebarsError> exp =
        hbs_.try_render_to(out, fileText, context, options);
    if (!exp)
    {
        return Unexpected(Error(exp.error().what()));
    }
    return {};
}

//------------------------------------------------

std::string
Builder::
getRelPrefix(std::size_t depth)
{
    Config const& config = domCorpus->config;

    std::string rel_prefix;
    if(! depth || ! config->legibleNames ||
        ! domCorpus->config->multipage)
        return rel_prefix;
    --depth;
    rel_prefix.reserve(depth * 3);
    while(depth--)
        rel_prefix.append("../");
    return rel_prefix;
}

dom::Object
Builder::
createContext(
    Info const& I)
{
    dom::Object ctx;
    ctx.set("symbol", domCorpus.get(I.id));
    ctx.set("config", domCorpus->config.object());
    return ctx;
}

template <std::derived_from<Info> T>
Expected<void>
Builder::
operator()(std::ostream& os, T const& I)
{
    std::string const templateFile = fmt::format("index.{}.hbs", domCorpus.fileExtension);
    dom::Object const ctx = createContext(I);

    if (auto& config = domCorpus->config;
        config->embedded ||
        !config->multipage)
    {
        // Single page or embedded pages render the index template directly
        // without the wrapper
        return callTemplate(os, templateFile, ctx);
    }

    // Multipage output: render the wrapper template
    // The context receives the original symbol and the contents from rendering
    // the index template
    auto const wrapperFile = fmt::format("wrapper.{}.hbs", domCorpus.fileExtension);
    dom::Object const wrapperCtx = createFrame(ctx);
    wrapperCtx.set("contents", dom::makeInvocable([this, &I, templateFile, &os](
        dom::Value const&) -> Expected<dom::Value>
        {
            // Helper to write contents directly to stream
            MRDOCS_TRY(callTemplate(os, templateFile, createContext(I)));
            return {};
        }));
    return callTemplate(os, wrapperFile, wrapperCtx);
}

// Compile the Builder::operator() for each Info type
#define INFO(T) template Expected<void> Builder::operator()<T##Info>(std::ostream&, T##Info const&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

Expected<void>
Builder::
renderWrapped(
    std::ostream& os,
    std::function<Expected<void>()> contentsCb)
{
    auto const wrapperFile = fmt::format(
        "wrapper.{}.hbs", domCorpus.fileExtension);
    dom::Object ctx;
    ctx.set("contents", dom::makeInvocable([&](
        dom::Value const&) -> Expected<dom::Value>
    {
        MRDOCS_TRY(contentsCb());
        return {};
    }));

    // Render the wrapper directly to ostream
    auto pathName = files::appendPath(layoutDir(), wrapperFile);
    MRDOCS_TRY(auto fileText, files::getFileText(pathName));
    HandlebarsOptions options;
    options.escapeFunction = escapeFn_;
    OutputRef outRef(os);
    Expected<void, HandlebarsError> exp =
        hbs_.try_render_to(
            outRef, fileText, ctx, options);
    if (!exp)
    {
        Error(exp.error().what()).Throw();
    }
    return {};
}

std::string
Builder::
layoutDir() const
{
    return templatesDir("layouts");
}

std::string
Builder::
templatesDir() const
{
    Config const& config = domCorpus->config;
    return files::appendPath(
        config->addons,
        "generator",
        domCorpus.fileExtension);
}

std::string
Builder::
templatesDir(std::string_view subdir) const
{
    Config const& config = domCorpus->config;
    return files::appendPath(
        config->addons,
        "generator",
        domCorpus.fileExtension,
        subdir);
}

std::string
Builder::
commonTemplatesDir() const
{
    Config const& config = domCorpus->config;
    return files::appendPath(
        config->addons,
        "generator",
        "common");
}

std::string
Builder::
commonTemplatesDir(std::string_view subdir) const
{
    Config const& config = domCorpus->config;
    return files::appendPath(
        config->addons,
        "generator",
        "common",
        subdir);
}


} // hbs
} // mrdocs
} // clang
