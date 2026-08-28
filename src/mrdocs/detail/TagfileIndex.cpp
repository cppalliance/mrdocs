//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/detail/TagfileIndex.hpp>
#include <utility>

namespace mrdocs::detail {

bool
TagfileIndex::
insert(
    std::string_view qualifiedName,
    std::string url)
{
    bool const usable =
        !qualifiedName.empty() &&
        !url.empty() &&
        !contains(qualifiedName);
    if (usable)
    {
        urls_.emplace(std::string(qualifiedName), std::move(url));
    }
    return usable;
}

bool
TagfileIndex::
contains(std::string_view qualifiedName) const
{
    return urls_.contains(qualifiedName);
}

std::optional<std::string>
TagfileIndex::
find(std::string_view qualifiedName) const
{
    std::optional<std::string> result;
    std::map<std::string, std::string, std::less<>>::const_iterator const it =
        urls_.find(qualifiedName);
    if (it != urls_.end())
    {
        result = it->second;
    }
    return result;
}

bool
TagfileIndex::
empty() const noexcept
{
    return urls_.empty();
}

std::size_t
TagfileIndex::
size() const noexcept
{
    return urls_.size();
}

} // mrdocs::detail
