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
#include <atomic>

namespace clang::mrdocs::hbs {

/** Visitor which emites a multi-page reference.
*/
class MultiPageVisitor
{
    ExecutorGroup<Builder>& ex_;
    std::string_view outputPath_;
    Corpus const& corpus_;
    std::atomic<std::size_t> count_ = 0;

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
    template <class T>
    requires std::derived_from<T, Info> || std::same_as<T, OverloadSet>
    void
    operator()(T const& I);

    /** Get number of pages generated.
    */
    std::size_t
    count() const noexcept
    {
        return count_.load(std::memory_order::relaxed);
    }
};

} // clang::mrdocs::hbs

#endif
