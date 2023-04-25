//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_ADOC_ADOCPAGESBUILDER_HPP
#define MRDOX_API_ADOC_ADOCPAGESBUILDER_HPP

#include "SafeNames.hpp"
#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <llvm/ADT/SmallString.h>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocPagesBuilder
    : public Corpus::Visitor
{
    Corpus const& corpus_;
    Reporter& R_;
    SafeNames names_;
    llvm::StringRef outputPath_;
    Config::WorkGroup wg_;

public:
    struct Page
    {
        llvm::SmallString<0> fileName;

        explicit
        Page(
            llvm::StringRef s)
            : fileName(s)
        {
        }
    };

    std::vector<Page> pages;

    AdocPagesBuilder(
        llvm::StringRef outputPath,
        Corpus const& corpus,
        Reporter& R)
        : corpus_(corpus)
        , R_(R)
        , names_(corpus_)
        , outputPath_(outputPath)
        , wg_(corpus.config().get())
    {
    }

    llvm::Error build();

    template<class T>
    void build(T const& I);

    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visit(Overloads const&) override;
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;
};

} // adoc
} // mrdox
} // clang

#endif
