//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "HandlebarsCorpus.hpp"
#include "VisitorHelpers.hpp"
#include <iterator>
#include <llvm/Support/raw_ostream.h>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Support/RangeFor.hpp>
#include <mrdocs/Support/String.hpp>

namespace clang::mrdocs::hbs {

dom::Object
HandlebarsCorpus::
construct(Info const& I) const
{
    dom::Object obj = this->DomCorpus::construct(I);
    if (shouldGenerate(I, getCorpus().config))
    {
        obj.set("url", getURL(I));
        obj.set("anchor", names_.getQualified(I.id, '-'));
        return obj;
    }

    // If the URL is not available because it's a specialization
    // or dependency, we still want to generate the URL and anchor
    // for the primary template if it's part of the corpus.
    if (Info const* primaryInfo = findAlternativeURLInfo(getCorpus(), I))
    {
        MRDOCS_ASSERT(shouldGenerate(*primaryInfo, getCorpus().config));
        obj.set("url", getURL(*primaryInfo));
        obj.set("anchor", names_.getQualified(primaryInfo->id, '-'));
    }
    return obj;
}

std::string
HandlebarsCorpus::
getURL(Info const& I) const
{
    bool const multipage = getCorpus().config->multipage;
    char const prefix = multipage ? '/' : '#';
    char const delim = multipage ? '/' : '-';
    std::string href(1, prefix);
    href += names_.getQualified(I.id, delim);
    if (multipage)
    {
        href.append(".");
        href.append(fileExtension);
    }
    return href;
}

} // clang::mrdocs::hbs
