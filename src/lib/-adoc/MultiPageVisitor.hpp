//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_ADOC_MULTIPAGEVISITOR_HPP
#define MRDOCS_LIB_ADOC_MULTIPAGEVISITOR_HPP

#include "Builder.hpp"
#include <mrdocs/Support/ExecutorGroup.hpp>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace clang {
namespace mrdocs {
namespace adoc {

/** Visitor which emites a multi-page reference.
*/
class MultiPageVisitor
{
    ExecutorGroup<Builder>& ex_;
    std::string_view outputPath_;
    Corpus const& corpus_;

    void
    writePage(
        std::string_view text,
        std::string_view filename);

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
    void operator()(OverloadSet const& OS);
};

} // adoc
} // mrdocs
} // clang

#endif
