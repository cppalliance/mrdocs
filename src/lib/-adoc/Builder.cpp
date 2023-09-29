//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Builder.hpp"
#include "lib/Support/Radix.hpp"
#include <mrdox/Metadata/DomMetadata.hpp>
#include <mrdox/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>

namespace clang {
namespace mrdox {

namespace lua {
extern void lua_dump(dom::Object const& obj);
}

namespace adoc {

Builder::
Builder(
    AdocCorpus const& corpus)
    : domCorpus(corpus)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    Config const& config = domCorpus.getCorpus().config;

    js::Scope scope(ctx_);

    scope.script(files::getFileText(
        files::appendPath(
            config->addonsDir, "js", "handlebars.js")
        ).value()).maybeThrow();
    auto Handlebars = scope.getGlobal("Handlebars").value();

// VFALCO refactor this
Handlebars.setlog();

    // load templates
#if 0
    err = forEachFile(options_.template_dir,
        [&](std::string_view fileName)
        {
            if(! fileName.ends_with(".adoc.hbs"))
                return Error::success();
            return Error::success();
        });
#endif

    Error err;

    // load partials
    forEachFile(
        files::appendPath(config->addonsDir,
            "generator", "asciidoc", "partials"),
        [&](std::string_view pathName) -> Error
        {
            constexpr std::string_view ext = ".adoc.hbs";
            if(! pathName.ends_with(ext))
                return Error::success();
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            auto text = files::getFileText(pathName);
            if(! text)
                return text.error();
            return Handlebars.callProp("registerPartial", name, *text)
                .error_or(Error::success());
    }).maybeThrow();

    // load helpers
#if 0
    err = forEachFile(
        files::appendPath(config->addonsDir,
            "generator", "js", "helpers"),
        [&](std::string_view pathName)
        {
            constexpr std::string_view ext = ".js";
            if(! pathName.ends_with(ext))
                return Error::success();
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            //Handlebars.callProp("registerHelper", name, *text);
            auto err = ctx_.scriptFromFile(pathName);
            return Error::success();
        }).maybeThrow();
#endif
    scope.script(fmt::format(
        R"(Handlebars.registerHelper(
            'is_multipage', function()
        {{
            return {};
        }});
        )", config->multiPage)).maybeThrow();

    scope.script(R"(
        Handlebars.registerHelper(
            'to_string', function(context, depth)
        {
            return JSON.stringify(context, null, 2);
        });

        Handlebars.registerHelper(
            'eq', function(a, b)
        {
            return a === b;
        });

        Handlebars.registerHelper(
            'neq', function(a, b)
        {
            return a !== b;
        });

        Handlebars.registerHelper(
            'not', function(a)
        {
            return ! a;
        });

        Handlebars.registerHelper(
            'or', function(a, b)
        {
            return a || b;
        });

        Handlebars.registerHelper(
            'and', function(a, b)
        {
            return a && b;
        });
    )").maybeThrow();
}

//------------------------------------------------

Expected<std::string>
Builder::
callTemplate(
    std::string_view name,
    dom::Value const& context)
{
    Config const& config = domCorpus.getCorpus().config;

    js::Scope scope(ctx_);
    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config->addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, name);
    MRDOX_TRY(auto fileText, files::getFileText(pathName));
    dom::Object options;
    options.set("noEscape", true);
    options.set("allowProtoPropertiesByDefault", true);
    // VFALCO This makes Proxy objects stop working
    //options.set("allowProtoMethodsByDefault", true);
    MRDOX_TRY(auto templateFn, Handlebars->callProp("compile", fileText, options));
    MRDOX_TRY(auto result, templateFn.call(context, options));
    return result.getString();
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

dom::Value
Builder::
createContext(
    Info const& I)
{
    dom::Object::storage_type props;
    props.emplace_back("symbol", domCorpus.get(I.id));
    std::string rel_prefix;
    if(domCorpus.options.safe_names &&
        domCorpus.getCorpus().config->multiPage &&
        ! I.Namespace.empty())
    {
        auto depth = I.Namespace.size() - 1;
        rel_prefix.reserve(depth * 3);
        while(depth--)
            rel_prefix.append("../");
    }
    props.emplace_back("relfileprefix", std::move(rel_prefix));
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

} // adoc
} // mrdox
} // clang
