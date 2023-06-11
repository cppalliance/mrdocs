//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_ADOC_ADOCPAGESBUILDER_HPP
#define MRDOX_TOOL_ADOC_ADOCPAGESBUILDER_HPP

#include "Support/SafeNames.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/ThreadPool.hpp>
#include <llvm/ADT/SmallString.h>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocPagesBuilder
    : public Corpus::Visitor
{
    Corpus const& corpus_;
    SafeNames names_;
    llvm::StringRef outputPath_;
    TaskGroup taskGroup_;

public:
    AdocPagesBuilder(
        llvm::StringRef outputPath,
        Corpus const& corpus)
        : corpus_(corpus)
        , names_(corpus_)
        , outputPath_(outputPath)
        , taskGroup_(corpus.config.threadPool())
    {
    }

    Error build();

    template<class T>
    void build(T const& I);

    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;
};

} // adoc
} // mrdox
} // clang

#endif
