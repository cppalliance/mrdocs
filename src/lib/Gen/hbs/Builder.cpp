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
}

Builder::
Builder(
    HandlebarsCorpus const& corpus,
    std::function<void(OutputRef&, std::string_view)> escapeFn)
    : domCorpus(corpus)
    , escapeFn_(std::move(escapeFn))
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
            dom::Value src_loc = v.get("loc");
            if(! src_loc)
                return nullptr;
            dom::Value decls = src_loc.get("decl");
            if(dom::Value def = src_loc.get("def"))
            {
                // for classes/enums, prefer the definition
                dom::Value kind = v.get("kind");
                if(kind == "record" || kind == "enum")
                    return def;

                // we only every want to use the definition
                // for non-tag types when no other declaration
                // exists
                if(! decls)
                    return def;
            }
            if(! decls.isArray())
                return nullptr;
            dom::Value first;
            // otherwise, use whatever declaration had docs.
            // if no declaration had docs, fallback to the
            // first declaration
            for(const dom::Value& loc : decls.getArray())
            {
                if(loc.get("documented"))
                    return loc;
                else if(! first)
                    first = loc;
            }
            return first;
        }));

    helpers::registerConstructorHelpers(hbs_);
    helpers::registerStringHelpers(hbs_);
    helpers::registerAntoraHelpers(hbs_);
    helpers::registerLogicalHelpers(hbs_);
    helpers::registerContainerHelpers(hbs_);

    // load templates
    exp = forEachFile(layoutDir(), false,
        [&](std::string_view pathName) -> Expected<void>
        {
            // Get template relative path
            std::filesystem::path relPath = pathName;
            relPath = relPath.lexically_relative(layoutDir());

            // Skip non-handlebars files
            MRDOCS_CHECK_OR(relPath.extension() == ".hbs", {});

            // Load template contents
            MRDOCS_TRY(std::string text, files::getFileText(pathName));

            // Register template
            this->templates_.emplace(relPath.generic_string(), text);
            return {};
        });
    if (!exp)
    {
        exp.error().Throw();
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
    ctx.set("relfileprefix", getRelPrefix(I.Namespace.size()));
    ctx.set("config", domCorpus->config.object());
    ctx.set("sectionref", domCorpus.names_.getQualified(I.id, '-'));
    return ctx;
}

dom::Object
Builder::
createContext(
    OverloadSet const& OS)
{
    dom::Object ctx;
    ctx.set("symbol", domCorpus.getOverloads(OS));
    const Info& Parent = domCorpus->get(OS.Parent);
    ctx.set("relfileprefix", getRelPrefix(Parent.Namespace.size() + 1));
    ctx.set("config", domCorpus->config.object());
    ctx.set("sectionref", domCorpus.names_.getQualified(OS, '-'));
    return ctx;
}

template<class T>
requires std::derived_from<T, Info> || std::same_as<T, OverloadSet>
Expected<void>
Builder::
operator()(std::ostream& os, T const& I)
{
    std::string const templateFile =
        std::derived_from<T, Info> ?
            fmt::format("index.{}.hbs", domCorpus.fileExtension) :
            fmt::format("index-overload-set.{}.hbs", domCorpus.fileExtension);
    dom::Object ctx = createContext(I);

    auto& config = domCorpus->config;
    bool isSinglePage = !config->multipage;
    if (config->embedded ||
        isSinglePage)
    {
        return callTemplate(os, templateFile, ctx);
    }

    auto const wrapperFile = fmt::format("wrapper.{}.hbs", domCorpus.fileExtension);
    dom::Object wrapperCtx = createFrame(ctx);
    wrapperCtx.set("contents", dom::makeInvocable([this, &I, templateFile, &os](
        dom::Value const& options) -> Expected<dom::Value>
        {
            // Helper to write contents directly to stream
            MRDOCS_TRY(callTemplate(os, templateFile, createContext(I)));
            return {};
        }));
    return callTemplate(os, wrapperFile, wrapperCtx);
}

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
        dom::Value const& options) -> Expected<dom::Value>
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

// Define Builder::operator() for each Info type
#define INFO(T) template Expected<void> Builder::operator()<T##Info>(std::ostream&, T##Info const&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

template Expected<void> Builder::operator()<OverloadSet>(std::ostream&, OverloadSet const&);

} // hbs
} // mrdocs
} // clang
