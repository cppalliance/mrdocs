//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_HTML_BUILDER_HPP
#define MRDOX_LIB_HTML_BUILDER_HPP

#include "HTMLTag.hpp"
#include "Options.hpp"
#include "Support/Radix.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Error.hpp>
#include <ostream>

// #include <mrdox/Support/Dom.hpp>

namespace clang {
namespace mrdox {
namespace html {

/** Builds reference output.

    This contains all the state information
    for a single thread to generate output.
*/
class Builder
{
    Corpus const& corpus_;
    Options options_;
    // js::Context ctx_;

public:
    Builder(
        Corpus const& corpus,
        Options const& options);

    Expected<std::string> operator()(NamespaceInfo const&);
    Expected<std::string> operator()(RecordInfo const&);
    Expected<std::string> operator()(FunctionInfo const&);
    Expected<std::string> operator()(VariableInfo const&);
    Expected<std::string> operator()(TypedefInfo const&);

    HTMLTagWriter buildInfo(const Info&, bool primary = false);
    HTMLTagWriter buildInfo(const NamespaceInfo&, bool primary = false);
    HTMLTagWriter buildInfo(const RecordInfo&, bool primary = false);
    HTMLTagWriter buildInfo(const FunctionInfo&, bool primary = false);
    HTMLTagWriter buildInfo(const VariableInfo&, bool primary = false);
    HTMLTagWriter buildInfo(const FieldInfo&, bool primary = false);
    HTMLTagWriter buildInfo(const TypedefInfo&, bool primary = false);

    void writeChildren(
        HTMLTagWriter&,
        const std::vector<SymbolID>&);

    std::string
    buildTypeInfo(const TypeInfo& I);

    std::string
    buildParam(const Param& P);

    std::string
    buildTParam(const TParam& P);

    std::string
    buildTParams(
        const std::vector<TParam>& params);

    void
    writeTemplateHead(
        HTMLTagWriter& tag,
        const std::unique_ptr<TemplateInfo>& I);

    std::string
    buildTemplateArg(
        const TArg& arg);

    std::string
    buildTemplateArgs(
        const std::unique_ptr<TemplateInfo>& I);

    void
    writeName(
        HTMLTagWriter& tag,
        const Info& I);

    template<typename InfoTy>
    void
    writeTemplateName(
        HTMLTagWriter& tag,
        const InfoTy& I)
            requires requires { I.Template; };

    void
    writeBrief(
        HTMLTagWriter& tag,
        const Info& I);

    void
    writeDescription(
        HTMLTagWriter& tag,
        const Info& I);

    // void insertMember(js::Array const&, auto const& I);
    // void makeJavadoc(js::Object const& item, Javadoc const& jd);
    // void renderDocNode(std::string& dest, doc::Node const& node);

    // std::string renderFormalParam(Param const& I);
    // std::string renderTypeName(TypeInfo const& I);
    // std::string renderFunctionDecl(FunctionInfo const&);

    //--------------------------------------------

    // dom::Object domGetSymbol(SymbolID const& id);
};

} // html
} // mrdox
} // clang

#endif
