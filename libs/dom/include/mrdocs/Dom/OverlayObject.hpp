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

#ifndef MRDOCS_API_DOM_OVERLAYOBJECT_HPP
#define MRDOCS_API_DOM_OVERLAYOBJECT_HPP

#include <mrdocs/Dom/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

namespace mrdocs {
namespace dom {

/** An object whose keys are layered over the keys of a parent.

    The keys of the child shadow the keys of the parent: a read finds the
    child's value first, and falls back to the parent for any other key.
    A write goes to the child, so the parent is never changed. @ref size
    and @ref visit count each key once, even when both layers have it, and
    @ref visit gives the keys of the child first.

    An overlay of an overlay is flattened into one list of layers, so a
    chain of any depth costs one level of lookup per layer and never a
    chain of nested calls.

    @code
    dom::Object parent;
    parent.set("a", 1);
    parent.set("b", 2);
    dom::Object child;
    child.set("b", 20);
    dom::Object const frame = dom::overlayObject(child, parent);
    // frame.get("a") is 1, frame.get("b") is 20, frame.size() is 2
    @endcode

    @see overlayObject
*/
class MRDOCS_DOM_DECL
    OverlayObjectImpl : public ObjectImpl
{
    std::vector<Object> grandParents_;
    Object parent_;
    Object child_;

public:
    /** Destructor.
    */
    ~OverlayObjectImpl() override;

    /** Create an overlay with an empty child.

        A write to the overlay goes to a child of its own, and the parent
        keeps its keys.

        @param parent The object the overlay falls back to.
    */
    explicit
    OverlayObjectImpl(Object parent);

    /** Create an overlay of a child over a parent.

        @param child The object whose keys shadow the parent's, and which
        receives the writes.
        @param parent The object the overlay falls back to.
    */
    OverlayObjectImpl(Object child, Object parent);

    /// @copydoc ObjectImpl::size
    std::size_t
    size() const override;

    /// @copydoc ObjectImpl::get
    Value
    get(std::string_view key) const override;

    /// @copydoc ObjectImpl::set
    void
    set(String key, Value value) override;

    /// @copydoc ObjectImpl::visit
    bool
    visit(std::function<bool(String, Value)> fn) const override;

    /// @copydoc ObjectImpl::exists
    bool
    exists(std::string_view key) const override;
};

/** Return an object that falls back to a parent for the keys it lacks.

    @param parent The object the overlay falls back to.
    @return An overlay with an empty child, as @ref OverlayObjectImpl makes.
*/
MRDOCS_DOM_DECL
Object
overlayObject(Object const& parent);

/** Return an object whose keys shadow the keys of a parent.

    @param child The object whose keys shadow the parent's, and which
    receives the writes.
    @param parent The object the overlay falls back to.
    @return An overlay of `child` over `parent`.
*/
MRDOCS_DOM_DECL
Object
overlayObject(Object const& child, Object const& parent);

} // dom
} // mrdocs

#endif
