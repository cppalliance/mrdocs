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

namespace clang {
namespace mrdocs {
namespace hbs {

template<class T>
void
SinglePageVisitor::
operator()(T const& I)
{
    ex_.async([this, &I, page = numPages_++](Builder& builder)
    {
        if(auto r = builder(I))
            writePage(*r, page);
        else
            r.error().Throw();
    });
    if constexpr(
            T::isNamespace() ||
            T::isRecord() ||
            T::isEnum())
    {
        // corpus_.traverse(I, *this);
        corpus_.traverseOverloads(I, *this);
    }
}

void
SinglePageVisitor::
operator()(OverloadSet const& OS)
{
    ex_.async([this, OS, page = numPages_++](Builder& builder)
    {
        if(auto r = builder(OS))
            writePage(*r, page);
        else
            r.error().Throw();
        corpus_.traverse(OS, *this);
    });
}

// pageNumber is zero-based
void
SinglePageVisitor::
writePage(
    std::string pageText,
    std::size_t pageNumber)
{
    std::unique_lock<std::mutex> lock(mutex_);

    if(pageNumber > topPage_)
    {
        // defer this page
        if( pages_.size() <= pageNumber)
            pages_.resize(pageNumber + 1);
        pages_[pageNumber] = std::move(pageText);
        return;
    }

    // write contiguous pages
    for(;;)
    {
        {
            unlock_guard unlock(mutex_);
            os_.write(pageText.data(), pageText.size());
            ++pageNumber;
        }
        topPage_ = pageNumber;
        if(pageNumber >= pages_.size())
            return;
        if(! pages_[pageNumber])
            return;
        pageText = std::move(*pages_[pageNumber]);
        // VFALCO this is in theory not needed but
        // I am paranoid about the std::move of the
        // string not resulting in a deallocation.
        pages_[pageNumber].reset();
    }
}

#define DEFINE(T) template void \
    SinglePageVisitor::operator()<T>(T const&)

#define INFO(Type) DEFINE(Type##Info);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

} // hbs
} // mrdocs
} // clang
