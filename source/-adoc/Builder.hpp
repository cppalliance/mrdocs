//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_ADOC_BUILDER_HPP
#define MRDOX_LIB_ADOC_BUILDER_HPP

#include "Options.hpp"
#include "Support/Radix.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/JavaScript.hpp>
#include <ostream>

#include <mrdox/Support/Dom.hpp>

namespace clang {
namespace mrdox {
namespace adoc {

/** Builds reference output.

    This contains all the state information
    for a single thread to generate output.
*/
class Builder
{
    Corpus const& corpus_;
    Options options_;
    js::Context ctx_;

public:
    Builder(
        Corpus const& corpus,
        Options const& options);

    Expected<std::string> operator()(NamespaceInfo const&);
    Expected<std::string> operator()(RecordInfo const&);
    Expected<std::string> operator()(FunctionInfo const&);

    void insertMember(js::Array const&, auto const& I);
    void makeJavadoc(js::Object const& item, Javadoc const& jd);
    void renderDocNode(std::string& dest, doc::Node const& node);

    std::string renderFormalParam(Param const& I);
    std::string renderTypeName(TypeInfo const& I);
    std::string renderFunctionDecl(FunctionInfo const&);

    //--------------------------------------------

    dom::Object domGetSymbol(SymbolID const& id);
};

} // adoc
} // mrdox
} // clang

#endif
