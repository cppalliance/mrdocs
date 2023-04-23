//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_ADOC_PAGESBUILDER_HPP
#define MRDOX_LIB_ADOC_PAGESBUILDER_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <llvm/ADT/SmallString.h>

namespace clang {
namespace mrdox {
namespace adoc {

class PagesBuilder : Corpus::Visitor
{
    Corpus const& corpus_;

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

    PagesBuilder(
        Corpus const& corpus) noexcept
        : corpus_(corpus)
    {
    }

    void scan();

    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visit(Overloads const&) override;
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;

private:
    llvm::SmallString<0> filePrefix_;
};

} // adoc
} // mrdox
} // clang

#endif
