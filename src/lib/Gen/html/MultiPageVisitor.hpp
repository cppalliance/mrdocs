//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HTML_MULTIPAGEVISITOR_HPP
#define MRDOCS_LIB_GEN_HTML_MULTIPAGEVISITOR_HPP

#include "Builder.hpp"
#include <mrdocs/Support/ExecutorGroup.hpp>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace clang {
namespace mrdocs {
namespace html {

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

    template<class T>
    void operator()(T const& I);
    void renderPage(auto const& I);
};

} // html
} // mrdocs
} // clang

#endif
