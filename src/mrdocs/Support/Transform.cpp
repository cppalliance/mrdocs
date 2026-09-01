//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Transform.hpp>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace mrdocs {

Transform::
~Transform() noexcept = default;

namespace {

/*  The global registry of installed transforms.

    Installing takes the lock, and applying takes it only to copy the
    list out: a transform runs with the lock released, so one that calls
    back into MrDocs cannot deadlock against the registry.
*/
class TransformRegistry
{
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Transform>> list_;

public:
    Expected<void>
    insert(std::unique_ptr<Transform> T)
    {
        MRDOCS_CHECK(T, "cannot install null transform");
        std::lock_guard<std::mutex> lock(mutex_);
        list_.emplace_back(std::move(T));
        return {};
    }

    std::vector<Transform const*>
    installed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Transform const*> result;
        result.reserve(list_.size());
        for (std::unique_ptr<Transform> const& T : list_)
        {
            result.push_back(T.get());
        }
        return result;
    }
};

TransformRegistry&
getTransformRegistry() noexcept
{
    static TransformRegistry impl;
    return impl;
}

} // (anon)

Expected<void>
installTransform(std::unique_ptr<Transform> T)
{
    return getTransformRegistry().insert(std::move(T));
}

Expected<void>
applyTransforms(Corpus& corpus, Config const& config)
{
    Expected<void> result;
    for (Transform const* T : getTransformRegistry().installed())
    {
        result = T->apply(corpus, config);
        if (!result)
        {
            result = Unexpected(formatError(
                "the transform \"{}\" failed: {}",
                T->id(), result.error().reason()));
            break;
        }
    }
    return result;
}

} // mrdocs
