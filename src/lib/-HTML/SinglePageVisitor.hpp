//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_HTML_SINGLEPAGEVISITOR_HPP
#define MRDOCS_LIB_HTML_SINGLEPAGEVISITOR_HPP

#include "Builder.hpp"
#include <mrdocs/Support/ExecutorGroup.hpp>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace clang {
namespace mrdocs {
namespace html {

/** Visitor which writes everything to a single page.
*/
class SinglePageVisitor
{
    ExecutorGroup<Builder>& ex_;
    Corpus const& corpus_;
    std::ostream& os_;
    std::size_t numPages_ = 0; 
    std::mutex mutex_;
    std::size_t topPage_ = 0;
    std::vector<std::optional<
        std::string>> pages_;

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

    template<class T>
    void operator()(T const& I);
    void renderPage(auto const& I, std::size_t pageNumber);
    void endPage(std::string pageText, std::size_t pageNumber);
};

} // html
} // mrdocs
} // clang

#endif
