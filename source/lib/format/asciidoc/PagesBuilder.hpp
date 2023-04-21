//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SOURCE_FORMAT_ASCIIDOC_PAGESBUILDER_HPP
#define MRDOX_SOURCE_FORMAT_ASCIIDOC_PAGESBUILDER_HPP

#include <mrdox/Corpus.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <llvm/ADT/SmallString.h>

namespace clang {
namespace mrdox {

class PagesBuilder : Corpus::Visitor
{
    Corpus const& corpus_;

public:
    struct Page
    {
        llvm::SmallString<0> fileName;
    };

    std::vector<Page> pages;

    PagesBuilder(
        Corpus const& corpus) noexcept
        : corpus_(corpus)
    {
    }

    void scan();

    void visit(NamespaceInfo const&) override;
    void visit(RecordInfo const&) override;
    void visit(FunctionOverloads const&) override;
    void visit(FunctionInfo const&) override;
    void visit(TypedefInfo const&) override;
    void visit(EnumInfo const&) override;

private:
    llvm::SmallString<0> filePrefix_;
};

} // mrdox
} // clang

#endif
