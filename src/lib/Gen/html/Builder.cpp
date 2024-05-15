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

namespace html {

Builder::
Builder(
    DomCorpus const& domCorpus,
    Options const& options)
    : domCorpus_(domCorpus)
    , corpus_(domCorpus_.getCorpus())
    , options_(options)
{
    namespace fs = std::filesystem;

    Config const& config = corpus_.config;

    // load partials
    std::string partialsPath = files::appendPath(
        config->addonsDir, "generator", "html", "partials");
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
        config->addonsDir, "generator", "html", "helpers");
    forEachFile(helpersPath, true,
        [&](std::string_view pathName)-> Expected<void>
        {
            // Register JS helper function in the global object
            constexpr std::string_view ext = ".js";
            if (!pathName.ends_with(ext))
                return {};
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
    Config const& config = corpus_.config;

    js::Scope scope(ctx_);


    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config->addonsDir,
            "generator", "html", "layouts");
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
    return callTemplate("single-header.html.hbs", {});
}

Expected<std::string>
Builder::
renderSinglePageFooter()
{
    return callTemplate("single-footer.html.hbs", {});
}

//------------------------------------------------

std::string
Builder::
getRelPrefix(std::size_t depth)
{
    std::string rel_prefix;
    if(! depth ||! domCorpus_->config->multiPage)
        return rel_prefix;
    --depth;
    rel_prefix.reserve(depth * 3);
    while(depth--)
        rel_prefix.append("../");
    return rel_prefix;
}

//------------------------------------------------

dom::Value
Builder::
createContext(
    SymbolID const& id)
{
    return dom::Object({
        { "symbol", domCorpus_.get(id) }
        });
}

dom::Value
Builder::
createContext(
    OverloadSet const& OS)
{
    dom::Object::storage_type props;
    props.emplace_back("symbol",
        domCorpus_.getOverloads(OS));
    const Info& Parent = domCorpus_->get(OS.Parent);
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
        "single-symbol.html.hbs",
        createContext(I.id));
}

Expected<std::string>
Builder::
operator()(OverloadSet const& OS)
{
    return callTemplate(
        "overload-set.html.hbs",
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

} // html
} // mrdocs
} // clang
