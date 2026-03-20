//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Reflection/MergeReflectedType.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol/Friend.hpp>
#include <algorithm>
#include <vector>

namespace mrdocs {

void
merge(FriendInfo& I, FriendInfo&& Other)
{
    mergeReflected(I, Other);
}

void
merge(
    std::vector<FriendInfo>& dst,
    std::vector<FriendInfo>&& src)
{
    for (FriendInfo const& F : src)
    {
        auto it = std::ranges::find_if(dst, [&F](auto const& other) {
            return F.id == other.id;
        });
        if (it == dst.end())
        {
            dst.push_back(F);
        }
    }
}

} // mrdocs

