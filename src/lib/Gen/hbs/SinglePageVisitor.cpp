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
#include <mrdocs/Support/unlock_guard.hpp>
#include <sstream>

namespace clang {
namespace mrdocs {
namespace hbs {

template<class T>
requires std::derived_from<T, Info> || std::same_as<T, OverloadSet>
void
SinglePageVisitor::
operator()(T const& I0)
{
    // If T is an OverloadSet, we make a copy for the lambda because
    // these are temporary objects that don't live in the corpus.
    // Otherwise, the lambda will capture a reference to the corpus Info.
    auto Ref = [&I0] {
        if constexpr (std::derived_from<T, Info>)
        {
            return std::ref(I0);
        }
        else if constexpr (std::same_as<T, OverloadSet>)
        {
            return OverloadSet(I0);
        }
    }();
    ex_.async([this, Ref, symbolIdx = numSymbols_++](Builder& builder)
    {
        T const& I = Ref;

        // Output to an independent string first, then write to
        // the shared stream
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

    if constexpr (std::derived_from<T, Info>)
    {
        if constexpr(
            T::isNamespace() ||
            T::isRecord() ||
            T::isEnum())
        {
            corpus_.traverseOverloads(I0, *this);
        }
    }
    else if constexpr (std::same_as<T, OverloadSet>)
    {
        corpus_.traverse(I0, *this);
    }

}

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

#define INFO(T) template void SinglePageVisitor::operator()<T##Info>(T##Info const&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

template void SinglePageVisitor::operator()<OverloadSet>(OverloadSet const&);

} // hbs
} // mrdocs
} // clang
