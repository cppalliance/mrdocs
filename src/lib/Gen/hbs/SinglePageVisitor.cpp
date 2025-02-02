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

#include "SinglePageVisitor.hpp"
#include "VisitorHelpers.hpp"
#include <mrdocs/Support/unlock_guard.hpp>
#include <sstream>

namespace clang::mrdocs::hbs {

template <std::derived_from<Info> T>
void
SinglePageVisitor::
operator()(T const& I)
{
    MRDOCS_CHECK_OR(shouldGenerate(I));
    ex_.async([this, &I, symbolIdx = numSymbols_++](Builder& builder)
    {
        // Output to an independent string first (async), then write to
        // the shared stream (sync)
        std::stringstream ss;
        if(auto r = builder(ss, I))
        {
            writePage(ss.str(), symbolIdx);
        }
        else
        {
            r.error().Throw();
        }
    });
    Corpus::TraverseOptions opts = {.skipInherited = std::same_as<T, RecordInfo>};
    corpus_.traverse(opts, I, *this);
}

#define INFO(T) template void SinglePageVisitor::operator()<T##Info>(T##Info const&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

// pageNumber is zero-based
void
SinglePageVisitor::
writePage(
    std::string pageText,
    std::size_t symbolIdx)
{
    std::unique_lock<std::mutex> lock(mutex_);

    if (symbolIdx > topSymbol_)
    {
        // Defer this symbol
        if( symbols_.size() <= symbolIdx)
            symbols_.resize(symbolIdx + 1);
        symbols_[symbolIdx] = std::move(pageText);
        return;
    }

    // Write contiguous pages
    for(;;)
    {
        // Write the current symbol
        {
            unlock_guard unlock(mutex_);
            os_.write(
                pageText.data(),
                static_cast<std::streamsize>(pageText.size()));
            ++symbolIdx;
        }

        topSymbol_ = symbolIdx;
        if(symbolIdx >= symbols_.size())
        {
            // No deferred symbols to write
            return;
        }

        if(! symbols_[symbolIdx])
        {
            // The next symbol is not set yet
            return;
        }

        // Render the next deferred symbol
        pageText = std::move(*symbols_[symbolIdx]);
        // VFALCO this is in theory not needed, but
        // I am paranoid about the std::move of the
        // string not resulting in a deallocation.
        symbols_[symbolIdx].reset();
    }
}

} // clang::mrdocs::hbs
