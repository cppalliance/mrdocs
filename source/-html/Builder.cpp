//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
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

namespace {

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

} // anon

std::string
Builder::
buildParam(const Param& P)
{
    HTMLTagWriter param(buildTypeInfo(P.Type));
    if(! P.Name.empty())
        param.write(" ", P.Name);
    if(! P.Default.empty())
        param.write(" = ", P.Default);
    return param;
}

std::string
Builder::
buildTParams(
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
        join(std::views::transform(params,
            [this](const TParam& P) { return buildTParam(P); }), ", "),
        ">");
    return tag;
}

std::string
Builder::
buildTParam(
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
Builder::
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
Builder::
buildTemplateArg(
    const TArg& arg)
{
    return arg.Value;
}

std::string
Builder::
buildTemplateArgs(
    const std::unique_ptr<TemplateInfo>& I)
{
    HTMLTagWriter tag;
    if(I && I->specializationKind() !=
        TemplateSpecKind::Primary)
    {
        tag.write(
            "<",
            join(std::views::transform(I->Args,
                [this](const TArg& A) { return buildTemplateArg(A); }), ", "),
            ">");
    }
    return tag;
}

void
Builder::
writeName(
    HTMLTagWriter& tag,
    const Info& I)
{
    tag.write({
        .Name = "span",
        .Class = "info-name",
        .Content = I.Name
    });
}

template<typename InfoTy>
void
Builder::
writeTemplateName(
    HTMLTagWriter& tag,
    const InfoTy& I)
        requires requires { I.Template; }
{
    tag.write({
        .Name = "span",
        .Class = "info-name",
        .Content = I.Name + buildTemplateArgs(I.Template)
    });
}

void
Builder::
writeBrief(
    HTMLTagWriter& tag,
    const Info& I)
{
    if(! I.javadoc)
        return;
    if(auto* brief = I.javadoc->getBrief())
    {
        HTMLTagWriter div({
            .Name = "div",
            .Class = "jd-brief"
        });
        DocVisitor()(*brief, div);
        tag.write(div);
    }
}

void
Builder::
writeDescription(
    HTMLTagWriter& tag,
    const Info& I)
{
    if(! I.javadoc)
        return;
    const auto& blocks = I.javadoc->getBlocks();
    if(! blocks.empty())
    {
        HTMLTagWriter div({
            .Name = "div",
            .Class = "jd-description"
        });
        DocVisitor()(blocks, div);
        tag.write(div);
    }
}

std::string
Builder::
buildTypeInfo(const TypeInfo& I)
{
    if(I.id == SymbolID::zero ||
        ! corpus_.find(I.id))
        return HTMLTagWriter({
            .Name = "span",
            .Class = "type-info",
            .Content = I.Name
        });
    std::string id_href =
        fmt::format("#{}", toBase16(I.id));
    return HTMLTagWriter({
        .Name = "a",
        .Class = "type-info",
        .Attrs = {{"href", id_href}},
        .Content = I.Name
    });
}

HTMLTagWriter
Builder::
buildInfo(
    const Info&,
    bool primary)
{
    return HTMLTagWriter();
}

HTMLTagWriter
Builder::
buildInfo(
    const FunctionInfo& I,
    bool primary)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = primary ? toBase16(I.id) : "",
        .Class = "function-info"
    });
    writeBrief(div, I);
    writeTemplateHead(div, I.Template);
    writeSpec(div, I.specs1.explicitSpec);
    writeSpec(div, I.specs0.storageClass);
    writeSpec(div, I.specs0.constexprKind);

    div.write(buildTypeInfo(I.ReturnType), " ");

    writeTemplateName(div, I);

    div.write(
        "(",
        join(std::views::transform(I.Params,
            [this](const Param& P) { return buildParam(P); }), ", "),
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
buildInfo(
    const VariableInfo& I,
    bool primary)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = primary ? toBase16(I.id) : "",
        .Class = "variable-info"
    });
    writeBrief(div, I);
    writeTemplateHead(div, I.Template);
    writeSpec(div, I.specs.storageClass);

    div.write(buildTypeInfo(I.Type), " ");
    writeTemplateName(div, I);
    return div;
}

HTMLTagWriter
Builder::
buildInfo(
    const FieldInfo& I,
    bool primary)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = primary ? toBase16(I.id) : "",
        .Class = "field-info"
    });
    writeBrief(div, I);
    div.write(buildTypeInfo(I.Type), " ");
    writeName(div, I);
    return div;
}

HTMLTagWriter
Builder::
buildInfo(
    const TypedefInfo& I,
    bool primary)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = primary ? toBase16(I.id) : "",
        .Class = "typedef-info"
    });
    writeBrief(div, I);
    if(I.IsUsing)
    {
        writeTemplateHead(div, I.Template);
        div.write({
            .Name = "span",
            .Class = "kw-using",
            .Content = "using"
        }).write(" ");
        writeTemplateName(div, I);
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
    const RecordInfo& I,
    bool primary)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = primary ? toBase16(I.id) : "",
        .Class = {"record-info"}
    });
    writeBrief(div, I);
    writeTemplateHead(div, I.Template);
    div.write({
        .Name = "span",
        .Class = "kw-class-key",
        .Content = toString(I.KeyKind)
    }).write(" ");;

    writeTemplateName(div, I);

    return div;
}

HTMLTagWriter
Builder::
buildInfo(
    const NamespaceInfo& I,
    bool primary)
{
    HTMLTagWriter div({
        .Name = "div",
        .Id = primary ? toBase16(I.id) : "",
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

void
Builder::
writeChildren(
    HTMLTagWriter& tag,
    const std::vector<SymbolID>& children)
{
    HTMLTagWriter div({
        .Name = "div",
        .Class = "scope-members"
    });
    for(const SymbolID& id : children)
    {
        visit(corpus_.get(id),
            [&](const auto& C)
            {
                div.write(buildInfo(C));
            });
    }
    tag.write(div);
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
    HTMLTagWriter tag = buildInfo(I, true);
    writeChildren(tag, I.Members);
    return std::string(tag) + "<hr>";
}

Expected<std::string>
Builder::
operator()(RecordInfo const& I)
{
    HTMLTagWriter tag = buildInfo(I, true);
    writeDescription(tag, I);
    writeChildren(tag, I.Members);
    return std::string(tag) + "<hr>";
}

Expected<std::string>
Builder::
operator()(FunctionInfo const& I)
{
#if 0
    if(auto* parent = corpus_.find(I.Namespace.front());
        parent && parent->isRecord())
        return "";
#endif
    HTMLTagWriter tag = buildInfo(I, true);
    writeDescription(tag, I);
    return std::string(tag) + "<hr>";
}

Expected<std::string>
Builder::
operator()(VariableInfo const& I)
{
#if 0
    if(auto* parent = corpus_.find(I.Namespace.front());
        parent && parent->isRecord())
        return "";
#endif
    HTMLTagWriter tag = buildInfo(I, true);
    writeDescription(tag, I);
    return std::string(tag) + "<hr>";
}

Expected<std::string>
Builder::
operator()(TypedefInfo const& I)
{
#if 0
    if(auto* parent = corpus_.find(I.Namespace.front());
        parent && parent->isRecord())
        return "";
#endif

    HTMLTagWriter tag = buildInfo(I);
    writeDescription(tag, I);
    return std::string(tag) + "<hr>";
}

//------------------------------------------------


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
