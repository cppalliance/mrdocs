//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Builder.hpp"
#include "lib/Support/Radix.hpp"
#include <lib/Lib/ConfigImpl.hpp>
#include <mrdocs/Metadata/DomMetadata.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>
#include <filesystem>

namespace clang {
namespace mrdocs {

namespace lua {
extern void lua_dump(dom::Object const& obj);
}

namespace adoc {

Builder::
Builder(
    AdocCorpus const& corpus)
    : domCorpus(corpus)
{
    namespace fs = std::filesystem;

    Config const& config = domCorpus->config;

    // load partials
    std::string partialsPath = files::appendPath(
        config->addonsDir, "generator", "asciidoc", "partials");
    forEachFile(partialsPath, true,
        [&](std::string_view pathName) -> Error
        {
            fs::path path = pathName;
            if(path.extension() != ".hbs")
                return Error::success();
            path = path.lexically_relative(partialsPath);
            while(path.has_extension())
                path.replace_extension();

            auto text = files::getFileText(pathName);
            if (! text)
                return text.error();

            hbs_.registerPartial(
                path.generic_string(), *text);
            return Error::success();
    }).maybeThrow();

    // Load JavaScript helpers
    std::string helpersPath = files::appendPath(
        config->addonsDir, "generator", "asciidoc", "helpers");
    forEachFile(helpersPath, true,
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
        }).maybeThrow();

    hbs_.registerHelper(
        "is_multipage",
        dom::makeInvocable([res = config->multiPage]() -> Expected<dom::Value> {
        return res;
    }));

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

    helpers::registerStringHelpers(hbs_);
    helpers::registerAntoraHelpers(hbs_);
    helpers::registerContainerHelpers(hbs_);
}

//------------------------------------------------

Expected<std::string>
Builder::
callTemplate(
    std::string_view name,
    dom::Value const& context)
{
    Config const& config = domCorpus->config;

    auto layoutDir = files::appendPath(config->addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, name);
    MRDOCS_TRY(auto fileText, files::getFileText(pathName));
    HandlebarsOptions options;
    options.noEscape = true;
    Expected<std::string, HandlebarsError> exp =
        hbs_.try_render(fileText, context, options);
    if (!exp)
    {
        return Unexpected(Error(exp.error().what()));
    }
    return *exp;
}

Expected<std::string>
Builder::
renderSinglePageHeader()
{
    return callTemplate("single-header.adoc.hbs", {});
}

Expected<std::string>
Builder::
renderSinglePageFooter()
{
    return callTemplate("single-footer.adoc.hbs", {});
}

//------------------------------------------------


std::string
Builder::
getRelPrefix(std::size_t depth)
{
    std::string rel_prefix;
    if(! depth || ! domCorpus.options.safe_names ||
        ! domCorpus->config->multiPage)
        return rel_prefix;
    --depth;
    rel_prefix.reserve(depth * 3);
    while(depth--)
        rel_prefix.append("../");
    return rel_prefix;
}

dom::Value
Builder::
createContext(
    Info const& I)
{
    dom::Object::storage_type props;
    props.emplace_back("symbol",
        domCorpus.get(I.id));
    props.emplace_back("relfileprefix",
        getRelPrefix(I.Namespace.size()));
    props.emplace_back("config", domCorpus->config.object());
    return dom::Object(std::move(props));
}

dom::Value
Builder::
createContext(
    OverloadSet const& OS)
{
    dom::Object::storage_type props;
    props.emplace_back("symbol",
        domCorpus.getOverloads(OS));
    const Info& Parent = domCorpus->get(OS.Parent);
    props.emplace_back("relfileprefix",
        getRelPrefix(Parent.Namespace.size() + 1));
    return dom::Object(std::move(props));
}

template<class T>
Expected<std::string>
Builder::
operator()(T const& I)
{
    return callTemplate(
        "single-symbol.adoc.hbs",
        createContext(I));
}

Expected<std::string>
Builder::
operator()(OverloadSet const& OS)
{
    return callTemplate(
        "overload-set.adoc.hbs",
        createContext(OS));
}

#define DEFINE(T) template Expected<std::string> \
    Builder::operator()<T>(T const&)

DEFINE(NamespaceInfo);
DEFINE(RecordInfo);
DEFINE(FunctionInfo);
DEFINE(EnumInfo);
DEFINE(TypedefInfo);
DEFINE(VariableInfo);
DEFINE(FieldInfo);
DEFINE(SpecializationInfo);
DEFINE(FriendInfo);
DEFINE(EnumeratorInfo);
DEFINE(GuideInfo);
DEFINE(AliasInfo);
DEFINE(UsingInfo);

} // adoc
} // mrdocs
} // clang
