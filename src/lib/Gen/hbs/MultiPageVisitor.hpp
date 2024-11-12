//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HBS_MULTIPAGEVISITOR_HPP
#define MRDOCS_LIB_GEN_HBS_MULTIPAGEVISITOR_HPP

#include "Builder.hpp"
#include <mrdocs/Support/ExecutorGroup.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace clang {
namespace mrdocs {
namespace hbs {

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

    /** Push a task for the specified Info to the executor group.

        If the Info object refers to other Info objects, their
        respective tasks are also pushed to the executor group.

    */
    template <std::derived_from<Info> T>
    void operator()(T const& I);

    /** Push a task for the specified OverloadSet to the executor group.

        If the OverloadSet object refers to other Info objects, their
        respective tasks are also pushed to the executor group.

    */
    void operator()(OverloadSet const& OS);
};

} // hbs
} // mrdocs
} // clang

#endif
