//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "AdocSinglePageWriter.hpp"
#include "Support/Debug.hpp"
#include "Options.hpp"
#include <fmt/format.h>
#include "duktape.h"
#include <fstream>
#include <ranges>

namespace clang {
namespace mrdox {
namespace adoc {

AdocSinglePageWriter::
AdocSinglePageWriter(
    llvm::raw_ostream& os,
    Corpus const& corpus,
    Reporter& R) noexcept
    : AdocWriter(os, names_, corpus, R)
    , names_(corpus)
{
}

Err
AdocSinglePageWriter::
build()
{
    if(auto E = AdocWriter::init())
        return makeErr("init failed");
    Assert(sect_.level == 0);
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;
    llvm::SmallString<64> template_path(options_.template_dir);
    path::append(template_path, "file.adoc.hbs");
    if (!fs::exists(template_path) && path::is_relative(template_path)) {
        for(auto const& inputPath : InputPaths)
        {
            template_path = inputPath;
            path::append(template_path, options_.template_dir);
            path::append(template_path, "file.adoc.hbs");
            if (fs::exists(template_path))
                break;
        }
    }
    llvm::SmallString<64> handlebars_path;
    if (fs::exists(template_path)) {
        handlebars_path = template_path;
        auto dir = path::parent_path(handlebars_path);
        handlebars_path.resize(dir.size());
        path::append(handlebars_path, "handlebars.js");
        if (!fs::exists(handlebars_path)) {
            handlebars_path.clear();
        }
    }

    if (handlebars_path.empty()) {
        sect_.level = 1;
        sect_.markup = "=";
        os_ <<
            "= Reference\n"
            ":role: mrdox\n";
        corpus_.traverse(*this, SymbolID::zero);
        endSection();
    }
    else
    {
        duk_context *ctx = duk_create_heap_default();
        std::ifstream handlebars_file(handlebars_path.c_str());
        std::string handlebars_content((std::istreambuf_iterator<char>(handlebars_file)), std::istreambuf_iterator<char>());
        duk_eval_string_noresult(ctx, handlebars_content.c_str());
        duk_eval_string_noresult(ctx, "Handlebars.registerHelper('jsonPrettyPrint', function(context) {\n"
                             "  return JSON.stringify(context, null, 2);\n"
                             "});");
        duk_eval_string_noresult(ctx, "Handlebars.registerHelper('inc', function(value, options) {\n"
                             "  return parseInt(value) + 1;\n"
                             "});");

        ::duk_get_global_string(ctx, "Handlebars");
        ::duk_get_prop_string(ctx, -1, "compile");

        std::ifstream template_file(template_path.c_str());
        std::string template_content((std::istreambuf_iterator<char>(template_file)), std::istreambuf_iterator<char>());
        ::duk_push_string(ctx, template_content.c_str());
        ::duk_call(ctx, 1);

        // A visitor pushes JSON objects to the duktape stack
        struct duk_push_visitor : Visitor {
            AdocSinglePageWriter* that;
            duk_context *ctx;

            duk_push_visitor(AdocSinglePageWriter* that0, duk_context *ctx0)
                : Visitor(), that(that0), ctx(ctx0)
            {}

            bool
            visit(NamespaceInfo const& I) override
            {
                int ns_idx = ::duk_push_object(ctx);

                // Namespace name
                std::string s;
                if (I.id == SymbolID::zero) {
                    ::duk_push_string(ctx, "");
                } else {
                    std::string qs = I.getFullyQualifiedName(s);
                    ::duk_push_string(ctx, qs.c_str());
                }
                ::duk_put_prop_string(ctx, ns_idx, "name");

                auto push_children = [this, ns_idx]
                    (auto list, std::string_view field)
                {
                    int arr_idx = ::duk_push_array(ctx);
                    int i = 0;
                    for(auto const& I : list)
                    {
                        if(visit(I))
                        {
                            ::duk_put_prop_index(ctx, arr_idx, i++);
                        }
                        else
                        {
                            ::duk_pop_n(ctx, 1);
                            return false;
                        }
                    }
                    ::duk_put_prop_string(ctx, ns_idx, field.data());
                    return true;
                };

                auto dereference = [](auto const* v)
                    -> auto const &
                {
                    return *v;
                };

                auto ns = that->buildSortedList<NamespaceInfo>(
                    I.Children.Namespaces);
                auto nsView = std::ranges::views::transform(ns, dereference);
                push_children(nsView, "namespaces");

                auto recordList = that->buildSortedList<RecordInfo>(
                    I.Children.Records);
                auto recordListView = std::ranges::views::transform(recordList, dereference);
                push_children(recordListView, "classes");

                auto functionOverloads
                    = makeNamespaceOverloads(I, that->corpus_);
                push_children(functionOverloads.list, "overloads");

                auto typedefList = that->buildSortedList<TypedefInfo>(
                    I.Children.Typedefs);
                auto typedefListView = std::ranges::views::transform(typedefList, dereference);
                push_children(typedefListView, "typedefs");

                auto enumList = that->buildSortedList<EnumInfo>(
                    I.Children.Enums);
                auto enumListView = std::ranges::views::transform(enumList, dereference);
                push_children(enumListView, "enums");

                auto variableList = that->buildSortedList<VarInfo>(
                    I.Children.Vars);
                auto variableListView = std::ranges::views::transform(variableList, dereference);
                push_children(variableListView, "variables");

                return true;
            }

            bool
            push(int obj_idx, std::string_view key, std::string_view value)
            {
                llvm::SmallString<1024> safe_sv(value.begin(), value.end());
                ::duk_push_string(ctx, safe_sv.c_str());
                ::duk_put_prop_string(ctx, obj_idx, key.data());
                return true;
            }

            bool
            push(int obj_idx, std::string_view key, char const* value)
            {
                return push(obj_idx, key, std::string_view(value));
            }

            bool
            push(int obj_idx, std::string_view key, SmallVectorImpl<char> const& value)
            {
                return push(obj_idx, key, std::string_view(value.data(), value.size()));
            }

            bool
            push(int obj_idx, std::string_view key, Javadoc::Node const& value)
            {
                std::string s;
                llvm::raw_string_ostream ss(s);
                writeNode(ss, value);
                ::duk_push_string(ctx, ss.str().data());
                ::duk_put_prop_string(ctx, obj_idx, key.data());
                return true;
            }

            bool
            push(int obj_idx, std::string_view key, AnyList<Javadoc::Block> const& value)
            {
                std::string s;
                llvm::raw_string_ostream ss(s);
                writeNodes(ss, value);
                ::duk_push_string(ctx, ss.str().data());
                ::duk_put_prop_string(ctx, obj_idx, key.data());
                return true;
            }

            bool
            push(int obj_idx, std::string_view key, Javadoc::Node const* value)
            {
                if (value)
                    push(obj_idx, key, *value);
                else
                    push(obj_idx, key, "");
                return true;
            }

            bool
            push(int obj_idx, std::string_view key, bool value)
            {
                ::duk_push_boolean(ctx, value);
                ::duk_put_prop_string(ctx, obj_idx, key.data());
                return true;
            }


            bool
            visit(
                RecordInfo const& I) override
            {
                int obj_idx = ::duk_push_object(ctx);
                push(obj_idx, "id", that->names_.get(I.id));
                push(obj_idx, "name", I.Name);
                push(obj_idx, "xref", this->LinkFor(I).c_str());
                if (I.javadoc)
                {
                    push(obj_idx, "brief", I.javadoc->getBrief());
                    push(obj_idx, "description", I.javadoc->getBlocks());
                }
                else
                {
                    push(obj_idx, "brief", "");
                    push(obj_idx, "description", "");
                }
                if(I.DefLoc.has_value())
                    push(obj_idx, "filename", I.DefLoc->Filename);
                else if(! I.Loc.empty())
                    push(obj_idx, "filename", I.Loc[0].Filename);
                else
                    push(obj_idx, "filename", "");
                push(obj_idx, "tag", tagToString(I.TagType));

                int arr_idx = ::duk_push_array(ctx);
                for(std::size_t i = 0; i < I.Bases.size(); ++i)
                {
                    int base_idx = ::duk_push_object(ctx);
                    push(base_idx, "virtual", I.Bases[i].IsVirtual);
                    push(base_idx, "access", toString(I.Bases[i].access));
                    push(base_idx, "name", I.Bases[i].Name);
                    ::duk_put_prop_index(ctx, arr_idx, i);
                }
                ::duk_put_prop_string(ctx, obj_idx, "bases");

                auto push_briefs = [this](int obj_idx, std::string_view key, auto list)
                {
                    if(list.empty())
                        return;
                    int arr_idx = ::duk_push_array(ctx);
                    int i = 0;
                    for(auto const& V : list)
                    {
                        int el_idx = ::duk_push_object(ctx);
                        push(el_idx, "name", V.I->Name);
                        if (V.I->javadoc)
                            push(el_idx, "brief", V.I->javadoc->getBrief());
                        else
                            push(el_idx, "brief", "");
                        ::duk_put_prop_index(ctx, arr_idx, i++);
                    }
                    ::duk_put_prop_string(ctx, obj_idx, key.data());
                };

                auto J = makeInterface(I, that->corpus_);
                push_briefs(obj_idx, "classes",      J.Public.Records);
                push_briefs(obj_idx, "functions",    J.Public.Functions);
                push_briefs(obj_idx, "constants",    J.Public.Enums);
                push_briefs(obj_idx, "types",        J.Public.Types);
                push_briefs(obj_idx, "members",      J.Public.Data);
                push_briefs(obj_idx, "variables",    J.Public.Vars);

                int prot_obj = ::duk_push_object(ctx);
                push_briefs(prot_obj, "classes",     J.Protected.Records);
                push_briefs(prot_obj, "functions",   J.Protected.Functions);
                push_briefs(prot_obj, "constants",   J.Protected.Enums);
                push_briefs(prot_obj, "types",       J.Protected.Types);
                push_briefs(prot_obj, "members",     J.Protected.Data);
                push_briefs(prot_obj, "variables",   J.Protected.Vars);
                ::duk_put_prop_string(ctx, obj_idx, "protected");

                int priv_obj = ::duk_push_object(ctx);
                push_briefs(priv_obj, "classes",       J.Private.Records);
                push_briefs(priv_obj, "functions",     J.Private.Functions);
                push_briefs(priv_obj, "constants",     J.Private.Enums);
                push_briefs(priv_obj, "types",         J.Private.Types);
                push_briefs(priv_obj, "members",       J.Private.Data);
                push_briefs(priv_obj, "variables",     J.Private.Vars);
                ::duk_put_prop_string(ctx, obj_idx, "protected");

                return true;
            }

            bool
            visit(
                FunctionInfo const& I) override
            {
                int obj_idx = ::duk_push_object(ctx);
                push(obj_idx, "id", that->names_.get(I.id));
                push(obj_idx, "name", I.Name);
                push(obj_idx, "xref", this->LinkFor(I).c_str());
                if (I.javadoc)
                {
                    push(obj_idx, "brief", I.javadoc->getBrief());
                    push(obj_idx, "description", I.javadoc->getBlocks());
                }
                else
                {
                    push(obj_idx, "brief", "");
                    push(obj_idx, "description", "");
                }
                if(I.DefLoc.has_value()) {
                    push(obj_idx, "filename", I.DefLoc->Filename);
                } else if(! I.Loc.empty()) {
                    push(obj_idx, "filename", I.Loc[0].Filename);
                } else {
                    push(obj_idx, "filename", "");
                }
                push(obj_idx, "return", I.ReturnType.Name);

                int arr_idx = ::duk_push_array(ctx);
                for(std::size_t i = 0; i < I.Params.size(); ++i)
                {
                    int param_idx = ::duk_push_object(ctx);
                    push(param_idx, "name", I.Params[i].Name);
                    push(param_idx, "type", I.Params[i].Type.Name);
                    ::duk_put_prop_index(ctx, arr_idx, i);
                }
                ::duk_put_prop_string(ctx, obj_idx, "params");

                return true;
            }

            bool
            visit(
                TypedefInfo const& I) override
            {
                int obj_idx = ::duk_push_object(ctx);
                push(obj_idx, "id", that->names_.get(I.id));
                push(obj_idx, "name", I.Name);
                push(obj_idx, "xref", this->LinkFor(I).c_str());
                if (I.javadoc)
                {
                    push(obj_idx, "brief", I.javadoc->getBrief());
                    push(obj_idx, "description", I.javadoc->getBlocks());
                }
                else
                {
                    push(obj_idx, "brief", "");
                    push(obj_idx, "description", "");
                }
                if(I.DefLoc.has_value()) {
                    push(obj_idx, "filename", I.DefLoc->Filename);
                } else if(! I.Loc.empty()) {
                    push(obj_idx, "filename", I.Loc[0].Filename);
                } else {
                    push(obj_idx, "filename", "");
                }
                return true;
            }

            bool
            visit(
                EnumInfo const& I) override
            {
                int obj_idx = ::duk_push_object(ctx);
                push(obj_idx, "id", that->names_.get(I.id));
                push(obj_idx, "name", I.Name);
                push(obj_idx, "xref", this->LinkFor(I).c_str());
                if (I.javadoc)
                {
                    push(obj_idx, "brief", I.javadoc->getBrief());
                    push(obj_idx, "description", I.javadoc->getBlocks());
                }
                else
                {
                    push(obj_idx, "brief", "");
                    push(obj_idx, "description", "");
                }
                if(I.DefLoc.has_value()) {
                    push(obj_idx, "filename", I.DefLoc->Filename);
                } else if(! I.Loc.empty()) {
                    push(obj_idx, "filename", I.Loc[0].Filename);
                } else {
                    push(obj_idx, "filename", "");
                }
                return true;
            }

            bool visit(OverloadInfo const& I) {
                return visitOverloads(I);
            }

            bool visit(VarInfo const& I) override
            {
                int obj_idx = ::duk_push_object(ctx);
                push(obj_idx, "id", that->names_.get(I.id));
                push(obj_idx, "name", I.Name);
                push(obj_idx, "xref", this->LinkFor(I).c_str());
                if (I.javadoc)
                {
                    push(obj_idx, "brief", I.javadoc->getBrief());
                    push(obj_idx, "description", I.javadoc->getBlocks());
                }
                else
                {
                    push(obj_idx, "brief", "");
                    push(obj_idx, "description", "");
                }
                if(I.DefLoc.has_value()) {
                    push(obj_idx, "filename", I.DefLoc->Filename);
                } else if(! I.Loc.empty()) {
                    push(obj_idx, "filename", I.Loc[0].Filename);
                } else {
                    push(obj_idx, "filename", "");
                }
                return true;
            }

            bool
            visitOverloads(
                OverloadInfo const& I)
            {
                int arr_idx = ::duk_push_array(ctx);
                int i = 0;
                for(auto const& J : I.Functions)
                {
                    if(visit(*J))
                    {
                        ::duk_put_prop_index(ctx, arr_idx, i++);
                    }
                    else
                    {
                        ::duk_pop_n(ctx, 1);
                        return false;
                    }
                }
                return true;
            }

            std::string
            LinkFor(OverloadInfo const& I)
            {
                return fmt::format(
                    "xref:#{0}-{1}[{1}]",
                    that->names_.get(I.Parent->id).str(),
                    I.Name);
            }

            std::string
            LinkFor(Info const& I)
            {
                return fmt::format(
                    "xref:#{0}[{1}]",
                    that->names_.get(I.id).data(),
                    I.Name);
            }
        };

        duk_push_visitor visitor{this, ctx};
        corpus_.traverse(visitor, SymbolID::zero);

        ::duk_call(ctx, 1);
        std::string result = ::duk_get_string(ctx, -1);
        os_ << result;

        ::duk_destroy_heap(ctx);
    }
    return {};
}

//------------------------------------------------

template<class Type>
std::vector<Type const*>
AdocSinglePageWriter::
buildSortedList(
    std::vector<Reference> const& from) const
{
    std::vector<Type const*> result;
    result.reserve(from.size());
    for(auto const& ref : from)
        result.push_back(&corpus_.get<Type>(ref.id));
    llvm::sort(result,
        [&](Info const* I0, Info const* I1)
        {
            return compareSymbolNames(
                I0->Name, I1->Name) < 0;
        });
    return result;
}

/*  Write a namespace.

    This will index all individual
    symbols except child namespaces,
    sorted by group.
*/
bool
AdocSinglePageWriter::
visit(
    NamespaceInfo const& I)
{
    // build sorted list of namespaces,
    // this is for visitation not display.
    auto namespaceList     = buildSortedList<NamespaceInfo>(I.Children.Namespaces);
    auto recordList        = buildSortedList<RecordInfo>(I.Children.Records);
    auto functionOverloads = makeNamespaceOverloads(I, corpus_);
    auto typedefList       = buildSortedList<TypedefInfo>(I.Children.Typedefs);
    auto enumList          = buildSortedList<EnumInfo>(I.Children.Enums);
    auto variableList      = buildSortedList<VarInfo>(I.Children.Vars);

    // don't emit empty namespaces,
    // but still visit child namespaces.
    if( ! I.Children.Records.empty() ||
        ! functionOverloads.list.empty() ||
        ! I.Children.Typedefs.empty() ||
        ! I.Children.Enums.empty() ||
        ! I.Children.Vars.empty())
    {
        std::string s;
        if(I.id == SymbolID::zero)
        {
            s = "global namespace";
        }
        else
        {
            I.getFullyQualifiedName(s);
            s = "namespace " + s;
        }
        beginSection(s);

        if(! recordList.empty())
        {
            beginSection("Classes");
            os_ << "\n"
                "[cols=1]\n"
                "|===\n";
            for(auto const J : recordList)
            {
                os_ << "\n|";
                writeLinkFor(*J);
                os_ << '\n';
            };
            os_ << "|===\n";
            endSection();
        }

        if(! functionOverloads.list.empty())
        {
            beginSection("Functions");
            os_ << "\n"
                "[cols=1]\n"
                "|===\n";
            for(auto const& J : functionOverloads.list)
            {
                os_ << "\n|";
                writeLinkFor(J);
                os_ << '\n';
            };
            os_ << "|===\n";
            endSection();
        }

        if(! typedefList.empty())
        {
            beginSection("Types");
            os_ << "\n"
                "[cols=1]\n"
                "|===\n";
            for(auto const J : typedefList)
            {
                os_ << "\n|";
                writeLinkFor(*J);
                os_ << '\n';
            };
            os_ << "|===\n";
            endSection();
        }

        if(! enumList.empty())
        {
            beginSection("Enums");
            os_ << "\n"
                "[cols=1]\n"
                "|===\n";
            for(auto const J : enumList)
            {
                os_ << "\n|";
                writeLinkFor(*J);
                os_ << '\n';
            };
            os_ << "|===\n";
            endSection();
        }

        if(! variableList.empty())
        {
            beginSection("Vars");
            os_ << "\n"
                "[cols=1]\n"
                "|===\n";
            for(auto const J : variableList)
            {
                os_ << "\n|";
                writeLinkFor(*J);
                os_ << '\n';
            };
            os_ << "|===\n";
            endSection();
        }

        endSection();
    }

    // visit children
    for(auto const& I : namespaceList)
        if(! visit(*I))
            return false;
    for(auto const& I : recordList)
        if(! visit(*I))
            return false;
    for(auto const& I : functionOverloads.list)
        if(! visitOverloads(I))
            return false;
    for(auto const& I : typedefList)
        if(! visit(*I))
            return false;
    for(auto const& I : enumList)
        if(! visit(*I))
            return false;
    for(auto const& I : variableList)
        if(! visit(*I))
            return false;
    return true;
}

bool
AdocSinglePageWriter::
visit(
    RecordInfo const& I)
{
    write(I);
    return true;
}

bool
AdocSinglePageWriter::
visitOverloads(
    OverloadInfo const& I)
{
    // this is the "overload resolution landing page"
    // render the page

    // visit the functions
    for(auto const& J : I.Functions)
        if(! visit(*J))
            return false;
    return true;
}

bool
AdocSinglePageWriter::
visit(
    FunctionInfo const& I)
{
    write(I);
    return true;
}

bool
AdocSinglePageWriter::
visit(
    TypedefInfo const& I)
{
    write(I);
    return true;
}

bool
AdocSinglePageWriter::
visit(
    EnumInfo const& I)
{
    write(I);
    return true;
}

bool
AdocSinglePageWriter::
visit(
    VarInfo const&)
{
    return true;
}

#if 0
bool
AdocSinglePageWriter::
visitOverloads(
    Info const& P,
    OverloadInfo const& I)
{
    Assert(! I.Functions.empty());

    beginSection(P, I);

    // Location
    writeLocation(*I.Functions.front());

    // List of overloads
    os_ << '\n';
    for(auto const I : I.Functions)
    {
        os_ << ". `";
        writeFunctionDeclaration(*I);
        os_ << "`\n";
    };

    // Brief
    os_ << "\n//-\n";
    writeBrief(I.Functions.front()->javadoc, true);

    // List of descriptions
    for(auto const I : I.Functions)
    {
        os_ << ". ";
        if(I->javadoc)
            writeNodes(I->javadoc->getBlocks());
        else
            os_ << '\n';
    }

    endSection();

    return true;
}
#endif

} // adoc
} // mrdox
} // clang
