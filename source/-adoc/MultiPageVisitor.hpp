//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_ADOC_MULTIPAGEVISITOR_HPP
#define MRDOX_LIB_ADOC_MULTIPAGEVISITOR_HPP

#include "Builder.hpp"
#include <mrdox/Support/ExecutorGroup.hpp>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace clang {
namespace mrdox {
namespace adoc {

/** Visitor which emites a multi-page reference.
*/
class MultiPageVisitor
{
    ExecutorGroup<Builder>& ex_;
    std::string_view outputPath_;
    Corpus const& corpus_;

public:
    MultiPageVisitor(
        ExecutorGroup<Builder>& ex,
        std::string_view outputPath,
        Corpus const& corpus) noexcept
        : ex_(ex)
        , outputPath_(outputPath)
        , corpus_(corpus)
    {
    }

    void operator()(NamespaceInfo const& I);
    void operator()(RecordInfo const& I);
    void operator()(FunctionInfo const& I);
    void operator()(Info const&) {}

    void renderPage(auto const& I);
};

} // adoc
} // mrdox
} // clang

#endif
