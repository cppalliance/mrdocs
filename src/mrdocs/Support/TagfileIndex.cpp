//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/TagfileIndex.hpp>
#include <utility>

namespace mrdocs {

namespace {

// Join the parts of an external URL with exactly one separator between
// them.
std::string
makeUrl(TagfileIndex::Target const& target)
{
    std::string url = target.baseUrl;

    // A configured base URL may or may not end with a slash, while a
    // page name read from a tagfile never begins with one.
    if (!url.empty() &&
        !url.ends_with('/'))
    {
        url += '/';
    }
    url += target.page;
    if (!target.anchor.empty())
    {
        url += '#';
        url += target.anchor;
    }
    return url;
}

} // (anon)

bool
TagfileIndex::
insert(
    std::string_view qualifiedName,
    Target target)
{
    bool const usable =
        !qualifiedName.empty() &&
        !target.page.empty() &&
        !targets_.contains(qualifiedName);
    if (usable)
    {
        targets_.emplace(std::string(qualifiedName), std::move(target));
    }
    return usable;
}

std::optional<std::string>
TagfileIndex::
find(std::string_view qualifiedName) const
{
    std::optional<std::string> result;
    std::map<std::string, Target, std::less<>>::const_iterator const it =
        targets_.find(qualifiedName);
    if (it != targets_.end())
    {
        result = makeUrl(it->second);
    }
    return result;
}

bool
TagfileIndex::
empty() const noexcept
{
    return targets_.empty();
}

std::size_t
TagfileIndex::
size() const noexcept
{
    return targets_.size();
}

} // mrdocs
