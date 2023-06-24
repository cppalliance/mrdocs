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
#include "DocVisitor.hpp"
#include "Support/Radix.hpp"
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/String.hpp>
// #include <llvm/Support/FileSystem.h>
// #include <llvm/Support/Path.h>
#include <fmt/format.h>

#include <concepts>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {
namespace html {

namespace {

struct stringish
{
    stringish() = default;

    template<typename T>
        requires std::constructible_from<std::string_view, std::decay_t<T>>
    stringish(T&& t)
        : Underlying(std::string_view(std::forward<T>(t)))
    {
    }

    template<typename T>
        requires (! std::constructible_from<std::string_view, std::decay_t<T>> &&
            std::constructible_from<std::string, std::decay_t<T>>)
    stringish(T&& t)
        : Underlying(std::forward<T>(t))
    {
    }

    std::string Underlying;
};

struct HTMLTag
{
    std::string_view Name;
    std::string Id;
    std::vector<std::string_view> Class;
    std::vector<std::pair<
        std::string_view,
        std::string>> Attrs;
    stringish Content;
};

struct HTMLTagWriter
{
    HTMLTag Tag;

    HTMLTagWriter(
        const HTMLTag& tag)
        : Tag(tag)
    {
    }

    HTMLTagWriter(
        HTMLTag&& tag)
        : Tag(std::move(tag))
    {
    }

    HTMLTagWriter(
        std::string_view content)
        : HTMLTagWriter({.Content = content})
    {
    }

    template<typename... Args>
    HTMLTagWriter& write(
        std::string_view content,
        Args&&... args)
    {
        // write(content);
        Tag.Content.Underlying += content;
        stale_ = true;
        if constexpr(sizeof...(Args))
            write(std::forward<Args>(args)...);
        return *this;
    }

#if 0
    template<typename... Args>
    HTMLTagWriter& write(
        const HTMLTagWriter& child_tag,
        Args&&... args)
    {
        write(std::string(child_tag));
        if constexpr(sizeof...(Args))
            write(std::forward<Args>(args)...);
        return *this;
    }

    template<typename... Args>
    HTMLTagWriter& write(
        const HTMLTag& child_tag,
        Args&&... args)
    {
        write(HTMLTagWriter(child_tag));
        if constexpr(sizeof...(Args))
            write(std::forward<Args>(args)...);
        return *this;
    }
#else
    HTMLTagWriter& write(
        const HTMLTagWriter& child_tag)
    {
        write(std::string(child_tag));
        return *this;
    }

    HTMLTagWriter& write(
        const HTMLTag& child_tag)
    {
        write(HTMLTagWriter(child_tag));
        return *this;
    }
#endif

    operator std::string() const
    {
        if(stale_)
            return build();
        return cached_;
    }

    operator std::string&()
    {
        if(stale_)
        {
            cached_ = build();
            stale_ = false;
        }
        return cached_;
    }

    operator std::string_view()
    {
        if(stale_)
        {
            cached_ = build();
            stale_ = false;
        }
        return cached_;
    }

private:
    std::string build() const
    {
        if(Tag.Name.empty())
            return Tag.Content.Underlying;

        std::string r;
        r += fmt::format("<{}", Tag.Name);
        if(! Tag.Id.empty())
            r += fmt::format(" id = \"{}\"", Tag.Id);
        if(! Tag.Class.empty())
        {
            r += fmt::format(" class = \"{}", Tag.Class.front());
            for(auto c : std::span(Tag.Class.begin() + 1, Tag.Class.end()))
                r += fmt::format(" {}", c);
            r += '"';
        }
        for(auto&& [attr, val] : Tag.Attrs)
            r += fmt::format(" {} = \"{}\"", attr, val);
        if(Tag.Content.Underlying.empty())
            r += "/>";
        else
            r += fmt::format(">{}</{}>",
                Tag.Content.Underlying, Tag.Name);
        return r;
    }

    std::string cached_;
    bool stale_ = true;
};

std::string
buildIdHref(const SymbolID& id)
{
    return fmt::format("#{}", toBase16(id));
}

std::string
buildTypeInfo(const TypeInfo& I)
{
    if(I.id == SymbolID::zero)
        return HTMLTagWriter({
            .Name = "span",
            .Class = {"type-info"},
            .Content = I.Name
        });
    return HTMLTagWriter({
        .Name = "a",
        .Class = {"type-info"},
        .Attrs = {
            {"href", buildIdHref(I.id)}
        },
        .Content = I.Name
    });
}

std::string
buildParam(const Param& P)
{
    HTMLTagWriter param(buildTypeInfo(P.Type));
    if(! P.Name.empty())
        param.write(" ", P.Name);
    if(! P.Default.empty())
        param.write(" = ", P.Default);
    return param;
#if 0

    std::string r = buildTypeInfo(P.Type);
    if(! P.Name.empty())
        r += fmt::format(" {}", P.Name);
    if(! P.Default.empty())
        r += fmt::format(" = {}", P.Default);
    return r;
#endif
}

void
writeSpec(
    HTMLTagWriter& tag,
    StorageClassKind kind)
{
    if(kind == StorageClassKind::None)
        return;
    tag.write({
        .Name = "span",
        .Class = {"kw-storage-class-kind"},
        .Content = toString(kind)
    }).write(" ");
}

void
writeSpec(
    HTMLTagWriter& tag,
    ConstexprKind kind)
{
    if(kind == ConstexprKind::None)
        return;
    tag.write({
        .Name = "span",
        .Class = {"kw-constexpr-kind"},
        .Content = toString(kind)
    }).write(" ");
}


void
writeSpec(
    HTMLTagWriter& tag,
    ExplicitKind kind)
{
    if(kind == ExplicitKind::None)
        return;
    tag.write({
        .Name = "span",
        .Class = {"kw-explicit-kind"},
        .Content = toString(kind)
    }).write(" ");
}

void
writeSpec(
    HTMLTagWriter& tag,
    ReferenceKind kind)
{
    if(kind == ReferenceKind::None)
        return;
    std::string_view refqual =
        kind == ReferenceKind::LValue ?
        "&" : "&&";
    tag.write(" ").write({
        .Name = "span",
        .Class = {"tk-refqual-kind"},
        .Content = refqual
    });
}

void
writeSpec(
    HTMLTagWriter& tag,
    NoexceptKind kind)
{
    if(kind == NoexceptKind::None)
        return;
    tag.write(" ").write({
        .Name = "span",
        .Class = {"kw-noexcept-kind"},
        .Content = toString(kind)
    });
}

} // (anon)

std::string
Builder::
buildInfo(const FunctionInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = {"function-info"}
    });
    writeSpec(div, I.specs1.explicitSpec);
    writeSpec(div, I.specs0.storageClass);
    writeSpec(div, I.specs0.constexprKind);

    div.write(buildTypeInfo(I.ReturnType));

    div.write(
        " ",
        I.Name,
        "(",
        join(std::views::transform(I.Params, buildParam), ", "),
        ")");

    if(I.specs0.isConst)
        div.write(" ").write({
            .Name = "span",
            .Class = {"kw-const"},
            .Content = "const"});

    if(I.specs0.isVolatile)
        div.write(" ").write({
            .Name = "span",
            .Class = {"kw-volatile"},
            .Content = "volatile"});

    writeSpec(div, I.specs0.refQualifier);
    writeSpec(div, I.specs0.exceptionSpec);

    // div.write(")");
    return div;
}

std::string
Builder::
buildInfo(const FieldInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = {"field-info"}
    });
    div.write(buildTypeInfo(I.Type));
    div.write(" ", I.Name);
    return div;
}

std::string
Builder::
buildInfo(const VariableInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = {"function-info"}
    });
    writeSpec(div, I.specs.storageClass);
#if 0
    if(I.specs.storageClass != StorageClassKind::None)
    {
        div.write({
            .Name = "span",
            .Class = {"kw-storage-class"}
        }, toString(I.specs.storageClass));
        div.write(" ");
    }
#endif
    div.write(buildTypeInfo(I.Type));
    div.write(fmt::format(" {}", I.Name));
    return div;
}

std::string
Builder::
buildInfo(
    const RecordInfo& I,
    bool write_children)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = {"record-info"}
    });
    div.write({
        .Name = write_children ? "h1" : "span",
        .Content = fmt::format("class {}", I.Name)
    });

    if(write_children)
    {
        for(const SymbolID& id : I.Members)
        {
            visit(corpus_.get(id),
                [&](const auto& C)
                {
                    div.write(buildInfo(C));
                });
        }
    }
    return div;
}

std::string
Builder::
buildInfo(const Info&)
{
    return "";
}



Builder::
Builder(
    Corpus const& corpus,
    Options const& options)
    : corpus_(corpus)
    , options_(options)
{
#if 0
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    Config const& config = corpus.config;

    js::Scope scope(ctx_);

    {
        auto handlebarsPath = files::appendPath(
            config.addonsDir, "js", "handlebars.js");
        auto fileText = files::getFileText(handlebarsPath);
        if(! fileText)
            throw fileText.getError();
        if(auto err = scope.script(*fileText))
            throw err;
    }
    auto Handlebars = scope.getGlobal("Handlebars");
#endif

    // load templates
#if 0
    err = forEachFile(options_.template_dir,
        [&](std::string_view fileName)
        {
            if(! fileName.ends_with(".html.hbs"))
                return Error::success();
            return Error::success();
        });
#endif

#if 0
    Error err;

    // load partials
    err = forEachFile(
        files::appendPath(config.addonsDir,
            "generator", "asciidoc", "partials"),
        [&](std::string_view pathName)
        {
            constexpr std::string_view ext = ".html.hbs";
            if(! pathName.ends_with(ext))
                return Error::success();
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            auto text = files::getFileText(pathName);
            if(! text)
                return text.getError();
            Handlebars.call("registerPartial", name, *text);
            return Error::success();
        });
    if(err)
        throw err;
#endif

    // load helpers
#if 0
    err = forEachFile(
        files::appendPath(config.addonsDir,
            "generator", "js", "helpers"),
        [&](std::string_view pathName)
        {
            constexpr std::string_view ext = ".js";
            if(! pathName.ends_with(ext))
                return Error::success();
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            //Handlebars.call("registerHelper", name, *text);
            auto err = ctx_.scriptFromFile(pathName);
            return Error::success();
        });
    if(err)
        throw err;
#endif

#if 0
    err = scope.script(
        "Handlebars.registerHelper("
        "    'to_string', function(context)\n"
        "{\n"
        "   return JSON.stringify(context, null, 2);\n"
        "});");
    if(err)
        throw err;
#endif
}

Expected<std::string>
Builder::
operator()(NamespaceInfo const& I)
{
return std::string();
#if 0
    Config const& config = corpus_.config;

    js::Scope scope(ctx_);
    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config.addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, "namespace.html.hbs");
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.getError();
    js::Object options(scope);
    options.insert("noEscape", true);
    auto templateFn = Handlebars.call("compile", *fileText, options);

    js::Object context(scope);
    {
        // build page
        js::Object page(scope);
        page.insert("kind", "namespace");
        page.insert("name", I.Name);
        {
            js::Array list(scope);
            for(auto const& id : I.Members)
                visit(corpus_.get<Info>(id),
                    [&](auto const& I)
                    {
                        insertMember(list, I);
                    });
            page.insert("members", list);
        }
        context.insert("page", page);
    }

    auto result = js::tryCall(templateFn, context);
    if(! result)
        return result.getError();
    js::String htmlText(*result);
    return std::string(htmlText);
#endif
}

Expected<std::string>
Builder::
operator()(RecordInfo const& I)
{
    // return std::string();
    return buildInfo(I, true) + "<hr>";
#if 0
    Config const& config = corpus_.config;
    js::Scope scope(ctx_);
    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config.addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, "record.html.hbs");
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.getError();
    js::Object options(scope);
    options.insert("noEscape", true);
    auto templateFn = Handlebars.call("compile", *fileText, options);

    js::Object context(scope);
    {
        js::Object page(scope);
        page.insert("kind", "record");
        page.insert("name", I.Name);
        {
            js::Array list(scope);
            for(auto const& id : I.Members)
                visit(corpus_.get<Info>(id),
                    [&](auto const& I)
                    {
                        insertMember(list, I);
                    });
            page.insert("members", list);
        }
        context.insert("page", page);
    }

    auto result = js::tryCall(templateFn, context);
    if(! result)
        return result.getError();
    js::String htmlText(*result);
    return std::string(htmlText);
#endif
}

Expected<std::string>
Builder::
operator()(FunctionInfo const& I)
{
    Config const& config = corpus_.config;

#if 0
    return HTMLTagWriter({
        .Name = "div",
        .Id = toBase16(I.id)
    }, buildInfo(I));
#endif

    return buildInfo(I) + "<hr>";
#if 0
    js::Scope scope(ctx_);
    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config.addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, "function.html.hbs");
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.getError();
    js::Object options(scope);
    options.insert("noEscape", true);
    auto templateFn = Handlebars.call("compile", *fileText, options);

    js::Object context(scope);
    {
        js::Object page(scope);
        page.insert("kind", "record");
        page.insert("name", I.Name);
        page.insert("decl", renderFunctionDecl(I));
        if(I.DefLoc.has_value())
            page.insert("loc", I.DefLoc->Filename);
        if(I.javadoc)
            makeJavadoc(page, *I.javadoc);
        context.insert("page", page);
    }

    auto result = js::tryCall(templateFn, context);
    if(! result)
        return result.getError();
    js::String htmlText(*result);
    return std::string(htmlText);
#endif
}

//------------------------------------------------

#if 0
/*  Append the short-form descriptor of I to the list.
*/
void
Builder::
insertMember(
    js::Array const& list, auto const& I)
{
    js::Scope scope(ctx_);
    js::Object obj(scope);
    obj.insert("name", I.Name);
    obj.insert("tag", I.symbolType());
    obj.insert("id", toBase16(I.id));
    if(I.javadoc)
        makeJavadoc(obj, *I.javadoc);
    list.push_back(obj);
}
#endif

#if 0
void
Builder::
makeJavadoc(
    js::Object const& item,
    Javadoc const& jd)
{
    js::Scope scope(ctx_);
    js::Object obj(scope);

    std::string dest;

    // brief
    DocVisitor visitor(dest);
    dest.clear();
    if(auto brief = jd.getBrief())
    {
        visitor(*brief);
    }
    obj.insert("brief", dest);

    // description
    if(! jd.getBlocks().empty())
    {
        dest.clear();
        visitor(jd.getBlocks());
        if(! dest.empty())
            obj.insert("description", dest);
    }

    item.insert("doc", obj);
}
#endif

#if 0
std::string
Builder::
renderTypeName(
    TypeInfo const& I)
{
    if(I.id == SymbolID::zero)
        return I.Name;
    // VFALCO Work-around for having IDs that don't exist
    auto J = corpus_.find(I.id);
    if(J == nullptr)
        return I.Name;
    return J->Name;
}

std::string
Builder::
renderFormalParam(
    Param const& I)
{
    std::string s;
    s = renderTypeName(I.Type);
    if(! I.Name.empty())
    {
        s.push_back(' ');
        s.append(I.Name);
    }
    return s;
}

std::string
Builder::
renderFunctionDecl(
    FunctionInfo const& I)
{
    std::string dest;
    dest.append(renderTypeName(I.ReturnType));
    dest.push_back('\n');
    dest.append(I.Name);
    if(! I.Params.empty())
    {
        dest.append("(\n    ");
        dest.append(renderFormalParam(I.Params[0]));
        for(std::size_t i = 1; i < I.Params.size(); ++i)
        {
            dest.append(",\n    ");
            dest.append(renderFormalParam(I.Params[i]));
        }
        dest.append(");\n");
    }
    else
    {
        dest.append("();\n");
    }
    return dest;
}
#endif
//------------------------------------------------

#if 0
dom::Object
Builder::
domGetSymbol(
    SymbolID const& id)
{
    return visit(corpus_.get(id),
        [&]<class T>(T const& I)
        {
            return dom::makeObject<
                Symbol<T>>(I, corpus_);
        });
}
#endif

//------------------------------------------------

} // html
} // mrdox
} // clang
