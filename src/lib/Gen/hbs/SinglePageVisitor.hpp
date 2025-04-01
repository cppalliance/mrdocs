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

#ifndef MRDOCS_LIB_GEN_HBS_SINGLEPAGEVISITOR_HPP
#define MRDOCS_LIB_GEN_HBS_SINGLEPAGEVISITOR_HPP

#include "Builder.hpp"
#include <mrdocs/Support/ExecutorGroup.hpp>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace clang::mrdocs::hbs {

/** Visitor which writes everything to a single page.
*/
class SinglePageVisitor
{
    ExecutorGroup<Builder>& ex_;
    Corpus const& corpus_;
    std::ostream& os_;
    std::size_t numSymbols_ = 0;
    std::mutex mutex_;
    std::size_t topSymbol_ = 0;
    std::vector<std::optional<std::string>> symbols_;

    void writePage(std::string pageText, std::size_t symbolIdx);
public:
    SinglePageVisitor(
        ExecutorGroup<Builder>& ex,
        Corpus const& corpus,
        std::ostream& os) noexcept
        : ex_(ex)
        , corpus_(corpus)
        , os_(os)
    {
    }

    /** Push a task for the specified Info to the executor group.

        If the Info object refers to other Info objects, their
        respective tasks are also pushed to the executor group.

    */
    template <std::derived_from<Info> T>
    void operator()(T const& I);
};

} // clang::mrdocs::hbs

#endif
