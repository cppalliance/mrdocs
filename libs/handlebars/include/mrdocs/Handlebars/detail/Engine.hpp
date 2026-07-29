//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_DETAIL_ENGINE_HPP
#define MRDOCS_API_HANDLEBARS_DETAIL_ENGINE_HPP

// Internal types and helpers used by the Handlebars engine: the per-render
// state (defined in the engine source), the partial-template maps with
// heterogeneous lookup, and context-path manipulation. Internal to the
// handlebars library.

#include <mrdocs/Dom.hpp>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mrdocs {
namespace handlebars {
namespace detail {

/** Append a path segment to a context path (string id). */
inline
std::string
appendContextPath(dom::Value const& contextPath, std::string_view id)
{
    if (contextPath.isString())
    {
        return std::string(contextPath.getString()) + "." + std::string(id);
    }
    return std::string(id);
}

/** Append a path segment to a context path (dom value id). */
inline
std::string
appendContextPath(dom::Value const& contextPath, dom::Value const& id)
{
    if (!id.isString() && !id.isSafeString())
    {
        return std::string(contextPath);
    }
    return appendContextPath(contextPath, id.getString().get());
}

struct RenderState;

// Heterogeneous lookup support
struct string_hash {
    using is_transparent [[maybe_unused]] = void;
    size_t operator()(char const*txt) const {
        return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(std::string_view txt) const {
        return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(const std::string &txt) const {
        return std::hash<std::string>{}(txt);
    }
};

using partials_map = std::unordered_map<
    std::string, std::string, string_hash, std::equal_to<>>;

using partials_view_map = std::unordered_map<
    std::string, std::string_view, string_hash, std::equal_to<>>;

} // namespace detail
} // namespace handlebars
} // namespace mrdocs

#endif
