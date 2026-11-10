//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/OverlayObject.hpp>
#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace mrdocs {
namespace dom {

OverlayObjectImpl::
~OverlayObjectImpl() = default;

OverlayObjectImpl::
OverlayObjectImpl(Object parent)
{
    auto* parImpl = parent.impl().get();
    auto* parOverlay = dynamic_cast<OverlayObjectImpl*>(parImpl);
    if (parOverlay == nullptr)
    {
        parent_ = std::move(parent);
    }
    else if (parOverlay->child_.empty())
    {
        grandParents_ = parOverlay->grandParents_;
        parent_ = parOverlay->parent_;
    }
    else
    {
        grandParents_.push_back(parOverlay->parent_);
        grandParents_.insert(
            grandParents_.end(),
            parOverlay->grandParents_.begin(),
            parOverlay->grandParents_.end());
        parent_ = parOverlay->child_;
    }
}

OverlayObjectImpl::
OverlayObjectImpl(Object child, Object parent)
    : OverlayObjectImpl(std::move(parent))
{
    child_ = std::move(child);
    auto* childOverlay = dynamic_cast<OverlayObjectImpl*>(child_.impl().get());
    if (childOverlay != nullptr)
    {
        // The layers of the child come first, in their own order, and the
        // layers this overlay already had come after them.
        std::vector<Object> below;
        below.push_back(std::move(parent_));
        below.insert(
            below.end(),
            std::make_move_iterator(grandParents_.begin()),
            std::make_move_iterator(grandParents_.end()));
        grandParents_ = childOverlay->grandParents_;
        grandParents_.insert(
            grandParents_.end(),
            std::make_move_iterator(below.begin()),
            std::make_move_iterator(below.end()));
        parent_ = childOverlay->parent_;
        child_ = childOverlay->child_;
    }
}

// The keys of a layer that no layer below it has, added from the lowest layer
// up. The lowest layer is counted with its own size, so its values are never
// built; a higher layer is visited, and each of its keys is looked up in the
// layers below it with `exists`.
std::size_t
OverlayObjectImpl::
size() const
{
    std::vector<Object const*> layers;
    layers.push_back(&child_);
    layers.push_back(&parent_);
    for (Object const& grandParent : grandParents_)
    {
        layers.push_back(&grandParent);
    }
    std::size_t n = layers.back()->size();
    for (std::size_t i = layers.size() - 1; i-- > 0;)
    {
        n += layers[i]->size();
        layers[i]->visit([&](String const& key, Value const&)
        {
            for (std::size_t j = i + 1; j < layers.size(); ++j)
            {
                if (layers[j]->exists(key))
                {
                    --n;
                    break;
                }
            }
            return true;
        });
    }
    return n;
}

Value
OverlayObjectImpl::
get(std::string_view key) const
{
    if (child_.exists(key))
    {
        return child_.get(key);
    }
    if (parent_.exists(key))
    {
        return parent_.get(key);
    }
    for (auto const& grandParent : grandParents_)
    {
        if (grandParent.exists(key))
        {
            return grandParent.get(key);
        }
    }
    return Kind::Undefined;
}

void
OverlayObjectImpl::
set(String key, Value value)
{
    child_.set(key, std::move(value));
}

bool
OverlayObjectImpl::
visit(std::function<bool(String, Value)> fn) const
{
    if (!child_.visit(fn))
    {
        return false;
    }
    auto visit_if_not_inchild = [&](
        String const& key, Value const& value)
    {
        if (!child_.exists(key))
        {
            return fn(key, value);
        }
        return true;
    };
    if (!parent_.visit(visit_if_not_inchild))
    {
        return false;
    }

    for (std::size_t i = 0; i < grandParents_.size(); ++i)
    {
        auto visit_if_not_in_prev = [&](
            String const& key, Value const& value)
        {
            if (child_.exists(key))
            {
                return true;
            }
            if (parent_.exists(key))
            {
                return true;
            }
            for (std::size_t j = 0; j < i; ++j)
            {
                if (grandParents_[j].exists(key))
                {
                    return true;
                }
            }
            return fn(key, value);
        };
        if (!grandParents_[i].visit(visit_if_not_in_prev))
        {
            return false;
        }
    }
    return true;
}

bool
OverlayObjectImpl::
exists(std::string_view key) const
{
    if (child_.exists(key))
    {
        return true;
    }
    if (parent_.exists(key))
    {
        return true;
    }
    return std::ranges::any_of(
        grandParents_,
        [&](Object const& grandParent)
        {
            return grandParent.exists(key);
        });
}

Object
overlayObject(Object const& parent)
{
    return newObject<OverlayObjectImpl>(parent);
}

Object
overlayObject(Object const& child, Object const& parent)
{
    return newObject<OverlayObjectImpl>(child, parent);
}

} // dom
} // mrdocs
