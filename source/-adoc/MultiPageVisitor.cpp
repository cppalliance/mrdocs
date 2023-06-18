//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "MultiPageVisitor.hpp"

namespace clang {
namespace mrdox {
namespace adoc {

// Launch a task to render the page
void
MultiPageVisitor::
renderPage(
    auto const& I)
{
    ex_.async(
        [this, &I](Builder& builder)
        {
            auto pageText = builder(I);
            if(! pageText)
                throw pageText.getError();
        });
}

void
MultiPageVisitor::
operator()(NamespaceInfo const& I)
{
    renderPage(I);
    corpus_.traverse(I, *this);
}

void
MultiPageVisitor::
operator()(RecordInfo const& I)
{
    renderPage(I);
    corpus_.traverse(I, *this);
}

void
MultiPageVisitor::
operator()(FunctionInfo const& I)
{
    renderPage(I);
}

} // adoc
} // mrdox
} // clang
