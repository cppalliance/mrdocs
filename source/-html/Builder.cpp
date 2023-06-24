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

struct stringish
{
    stringish() = default;

    template<typename T>
        requires std::constructible_from<std::string_view, T>
    stringish(T&& t)
        : Underlying(std::string_view(std::forward<T>(t)))
    {
    }

    template<typename T>
        requires (! std::constructible_from<std::string_view, T> &&
            std::constructible_from<std::string, T>)
    stringish(T&& t)
        : Underlying(std::forward<T>(t))
    {
    }

    std::string Underlying;
};

template<typename T>
struct vectorish
{
    vectorish() = default;

    template<typename U>
        requires std::convertible_to<U, T>
    vectorish(U&& elem)
        : Underlying({elem})
    {
    }

    vectorish(const std::initializer_list<T>& init)
        : Underlying(init)
    {
    }

    std::vector<T> Underlying;
};

struct HTMLTag
{
    std::string_view Name;
    std::string Id;
    vectorish<std::string_view> Class;
    vectorish<std::pair<
        std::string_view,
        std::string>> Attrs;
    stringish Content;
};

struct HTMLTagWriter
{
    HTMLTag Tag;

    HTMLTagWriter() = default;

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
        const auto& Class = Tag.Class.Underlying;
        if(! Class.empty())
        {
            r += fmt::format(" class = \"{}", Class.front());
            for(auto c : std::span(Class.begin() + 1, Class.end()))
                r += fmt::format(" {}", c);
            r += '"';
        }
        for(auto&& [attr, val] : Tag.Attrs.Underlying)
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

namespace {

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
            .Class = "type-info",
            .Content = I.Name
        });
    return HTMLTagWriter({
        .Name = "a",
        .Class = "type-info",
        .Attrs = {{"href", buildIdHref(I.id)}},
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
        .Class = "kw-storage-class-kind",
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
        .Class = "kw-constexpr-kind",
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
        .Class = "kw-explicit-kind",
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
        .Class = "tk-refqual-kind",
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
        .Class = "kw-noexcept-kind",
        .Content = toString(kind)
    });
}

std::string buildTParam(
    const TParam& P);

// void writeTParams(
//     HTMLTagWriter& tag,
//     const std::vector<TParam>& params)
std::string buildTParams(
    const std::vector<TParam>& params)
{
    HTMLTagWriter tag;
    tag.write({
        .Name = "span",
        .Class = "kw-template",
        .Content = "template"
    });
    tag.write(
        "<",
        join(std::views::transform(params, buildTParam), ", "),
        ">");
    return tag;
}

std::string buildTParam(
    const TParam& P)
{
    HTMLTagWriter param;
    std::string default_val;
    switch(P.Kind)
    {
    case TParamKind::Type:
    {
        const auto& TP = P.get<TypeTParam>();
        param.write({
            .Name = "span",
            .Class = "kw-typename",
            .Content = "typename"
        });
        if(TP.Default)
            default_val = buildTypeInfo(*TP.Default);
        break;

    }
    case TParamKind::NonType:
    {
        const auto& NTTP = P.get<NonTypeTParam>();
        param.write(buildTypeInfo(NTTP.Type));
        if(NTTP.Default)
            default_val = *NTTP.Default;
        break;
    }
    case TParamKind::Template:
    {
        const auto& TTP = P.get<TemplateTParam>();
        param.write(buildTParams(TTP.Params), " ");
        param.write({
            .Name = "span",
            .Class = "kw-typename",
            .Content = "typename"
        });
        if(TTP.Default)
            default_val = *TTP.Default;
        break;
    }
    default:
        MRDOX_UNREACHABLE();
    }
    if(P.IsParameterPack)
        param.write("...");
    if(! P.Name.empty())
        param.write(" ", P.Name);
    if(! default_val.empty())
        param.write(" = ", default_val);
    return param;

}

void
writeTemplateHead(
    HTMLTagWriter& tag,
    const std::unique_ptr<TemplateInfo>& I)
{
    if(! I)
        return;
    tag.write({
        .Name = "div",
        .Class = "template-head",
        .Content = buildTParams(I->Params)
    });
}

std::string
buildTemplateArg(
    const TArg& arg)
{
    return arg.Value;
}

std::string
buildTemplateArgs(
    const std::unique_ptr<TemplateInfo>& I)
{
    HTMLTagWriter tag;
    if(I && I->specializationKind() !=
        TemplateSpecKind::Primary)
    {
        tag.write(
            "<",
            join(std::views::transform(I->Args, buildTemplateArg), ", "),
            ">");
    }
    return tag;
}

template<typename T>
void
writeName(
    HTMLTagWriter& tag,
    const T& t)
    requires (std::derived_from<T, Info> &&
        ! requires { t.Template; })
{
    tag.write({
        .Name = "span",
        .Class = "info-name",
        .Content = t.Name
    });
}

template<typename T>
void
writeName(
    HTMLTagWriter& tag,
    const T& t)
    requires std::derived_from<T, Info> &&
        requires { t.Template; }
{
    tag.write({
        .Name = "span",
        .Class = "info-name",
        .Content = t.Name + buildTemplateArgs(t.Template)
    });
}

} // (anon)

void
Builder::
writeChildren(
    HTMLTagWriter& tag,
    const std::vector<SymbolID>& children)
{
    for(const SymbolID& id : children)
    {
        visit(corpus_.get(id),
            [&](const auto& C)
            {
                tag.write(buildInfo(C));
            });
    }
}

HTMLTagWriter
Builder::
buildInfo(const FunctionInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = "function-info"
    });
    writeTemplateHead(div, I.Template);
    writeSpec(div, I.specs1.explicitSpec);
    writeSpec(div, I.specs0.storageClass);
    writeSpec(div, I.specs0.constexprKind);

    div.write(buildTypeInfo(I.ReturnType), " ");

    writeName(div, I);

    div.write(
        "(",
        join(std::views::transform(I.Params, buildParam), ", "),
        ")");

    if(I.specs0.isConst)
        div.write(" ").write({
            .Name = "span",
            .Class = "kw-const",
            .Content = "const"});

    if(I.specs0.isVolatile)
        div.write(" ").write({
            .Name = "span",
            .Class = "kw-volatile",
            .Content = "volatile"});

    writeSpec(div, I.specs0.refQualifier);
    writeSpec(div, I.specs0.exceptionSpec);

    return div;
}

HTMLTagWriter
Builder::
buildInfo(const VariableInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = "variable-info"
    });
    writeTemplateHead(div, I.Template);
    writeSpec(div, I.specs.storageClass);

    div.write(buildTypeInfo(I.Type), " ");
    writeName(div, I);
    return div;
}

HTMLTagWriter
Builder::
buildInfo(const FieldInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = "field-info"
    });
    div.write(buildTypeInfo(I.Type), " ");
    writeName(div, I);
    return div;
}

HTMLTagWriter
Builder::
buildInfo(const TypedefInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = "typedef-info"
    });
    if(I.IsUsing)
    {
        writeTemplateHead(div, I.Template);
        div.write({
            .Name = "span",
            .Class = "kw-using",
            .Content = "using"
        }).write(" ");
        writeName(div, I);
        div.write(" = ", buildTypeInfo(I.Underlying));
    }
    else
    {
        div.write({
            .Name = "span",
            .Class = "kw-typedef",
            .Content = "typedef"
        }).write(" ");
        div.write(buildTypeInfo(I.Underlying), " ");
        writeName(div, I);
    }
    // writeName(div, I);
    return div;
}

HTMLTagWriter
Builder::
buildInfo(
    const RecordInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = {"record-info"}
    });
    writeTemplateHead(div, I.Template);

    div.write({
        .Name = "span",
        .Class = "kw-class-key",
        .Content = toString(I.KeyKind)
    }).write(" ");;

    writeName(div, I);

    return div;
}

HTMLTagWriter
Builder::
buildInfo(
    const NamespaceInfo& I)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = toBase16(I.id),
        .Class = {"namespace-info"}
    });

    div.write({
        .Name = "span",
        .Class = "kw-namespace",
        .Content = "namespace"
    }).write(" ");;

    writeName(div, I);

    return div;
}

HTMLTagWriter
Builder::
buildInfo(const Info&)
{
    return HTMLTagWriter();
}



Builder::
Builder(
    Corpus const& corpus,
    Options const& options)
    : corpus_(corpus)
    , options_(options)
{

}

Expected<std::string>
Builder::
operator()(NamespaceInfo const& I)
{
    return std::string();
}

Expected<std::string>
Builder::
operator()(RecordInfo const& I)
{
    HTMLTagWriter tag = buildInfo(I);
    writeChildren(tag, I.Members);
    return std::string(tag) + "<hr>";
}

Expected<std::string>
Builder::
operator()(FunctionInfo const& I)
{
    if(auto* parent = corpus_.find(I.Namespace.front());
        parent && parent->isRecord())
        return "";

    HTMLTagWriter tag = buildInfo(I);
    return std::string(tag) + "<hr>";
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
