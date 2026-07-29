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

#include <mrdocs/Handlebars.hpp>
#include <cassert>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <format>
#include <print>
#include <ranges>
#include <unordered_set>
#include <optional>
#include <utility>


namespace mrdocs {
namespace handlebars {

// ==============================================================
// Utility functions
// ==============================================================

bool
isEmpty(dom::Value const& arg)
{
    if (arg.isArray())
    {
        return arg.getArray().empty();
    }
    if (arg.isInteger())
    {
        return false;
    }
    return !arg.isTruthy();
}

class OverlayObjectImpl : public dom::ObjectImpl
{
    std::vector<dom::Object> grandParents_;
    dom::Object parent_;
    dom::Object child_;

public:
    ~OverlayObjectImpl() override = default;

    OverlayObjectImpl(dom::Object parent)
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

    OverlayObjectImpl(dom::Object child, dom::Object parent)
        : OverlayObjectImpl(std::move(parent))
    {
        child_ = std::move(child);
        auto* childOverlay = dynamic_cast<OverlayObjectImpl*>(child_.impl().get());
        if (childOverlay != nullptr)
        {
            grandParents_.insert(
                grandParents_.begin(),
                parent_);
            grandParents_.insert(
                grandParents_.end(),
                childOverlay->grandParents_.begin(),
                childOverlay->grandParents_.end());
            parent_ = childOverlay->parent_;
            child_ = childOverlay->child_;
        }
    }

    std::size_t size() const override
    {
        std::size_t n = parent_.size() + child_.size();
        child_.visit([&](dom::String const& key, dom::Value const&)
        {
            if (parent_.exists(key))
            {
                --n;
            }
            else
            {
                for (auto const& grandParent : grandParents_)
                {
                    if (grandParent.exists(key))
                    {
                        --n;
                        break;
                    }
                }
            }
        });
        return n;
    };

    dom::Value get(std::string_view key) const override
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
        return dom::Kind::Undefined;
    }

    void set(dom::String key, dom::Value value) override
    {
        child_.set(key, std::move(value));
    };

    bool visit(std::function<bool(dom::String, dom::Value)> fn) const override
    {
        if (!child_.visit(fn))
        {
            return false;
        }
        auto visit_if_not_inchild = [&](
            dom::String const& key, dom::Value const& value)
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
                dom::String const& key, dom::Value const& value)
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

    bool exists(std::string_view key) const override
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
            [&](dom::Object const& grandParent)
            {
                return grandParent.exists(key);
            });
    }
};

dom::Object
createFrame(dom::Object const& parent)
{
    return dom::newObject<OverlayObjectImpl>(parent);
}

dom::Object
createFrame(dom::Object const& child, dom::Object const& parent)
{
    return dom::newObject<OverlayObjectImpl>(child, parent);
}

dom::Object
createFrame(dom::Value const& parent)
{
    if (parent.isObject())
    {
        return createFrame(parent.getObject());
    }
    return {};
}

void
escapeExpression(
    OutputRef out,
    std::string_view str,
    HandlebarsOptions const& opt)
{
    if (opt.noEscape)
    {
        out << str;
    }
    else
    {
        opt.escapeFunction(out, str);
    }
}

static void
format_to(
    OutputRef out,
    dom::Value const& value,
    HandlebarsOptions const& opt)
{
    if (value.isString())
    {
        escapeExpression(out, value.getString(), opt);
    }
    else if (value.isSafeString())
    {
        out << value.getString();
    }
    else if (value.isInteger())
    {
        out << value.getInteger();
    }
    else if (value.isBoolean())
    {
        if (value.getBool())
        {
            out << "true";
        }
        else
        {
            out << "false";
        }
    }
    else if (value.isArray())
    {
        out << "[";
        dom::Array const& array = value.getArray();
        if (!array.empty())
        {
            format_to(out, array.at(0), opt);
            dom::Array::size_type const n = array.size();
            for (std::size_t i = 1; i < n; ++i) {
                out << ",";
                format_to(out, array.at(i), opt);
            }
        }
        out << "]";
    }
    else if (value.isObject())
    {
        out << "[object Object]";
    }
}

static constexpr
std::string_view
trim_delimiters(std::string_view expression, std::string_view delimiters)
{
    auto pos = expression.find_first_not_of(delimiters);
    if (pos == std::string_view::npos)
    {
        return "";
    }
    expression.remove_prefix(pos);
    pos = expression.find_last_not_of(delimiters);
    if (pos == std::string_view::npos)
    {
        return "";
    }
    expression.remove_suffix(expression.size() - pos - 1);
    return expression;
}

static constexpr
std::string_view
trim_ldelimiters(std::string_view expression, std::string_view delimiters)
{
    auto pos = expression.find_first_not_of(delimiters);
    if (pos == std::string_view::npos)
    {
        return "";
    }
    expression.remove_prefix(pos);
    return expression;
}

static constexpr
std::string_view
trim_rdelimiters(std::string_view expression, std::string_view delimiters)
{
    auto pos = expression.find_last_not_of(delimiters);
    if (pos == std::string_view::npos)
    {
        return "";
    }
    expression.remove_suffix(expression.size() - pos - 1);
    return expression;
}

static constexpr
std::string_view
trim_spaces(std::string_view expression)
{
    return trim_delimiters(expression, " \t\r\n");
}

static constexpr
std::string_view
trim_lspaces(std::string_view expression)
{
    return trim_ldelimiters(expression, " \t\r\n");
}

static constexpr
std::string_view
trim_rspaces(std::string_view expression)
{
    return trim_rdelimiters(expression, " \t\r\n");
}

// ==============================================================
// Helper Callback
// ==============================================================

namespace detail {
    /* Holds the state information required for rendering templates.

        This structure contains various fields that are used to manage the state
        during the rendering process of Handlebars templates.
     */
    struct RenderState
    {
        /* The original template text.

           As the templateText being rendered changes,
           this is used for features that rely on the
           context of the template, such as finding
           the position of an error or identifying the
           context of a tag.

         */
        std::string_view rootTemplateText;

        /* The current template text being processed.

           This range of chars keeps changing as we
           render the template. For instance, when
           a tag contains ~, the string is updated
           the whitespaces around the tag.

         */
        std::string_view templateText;

        /* A vector of inline partials view maps.

           This vector is used to store maps of inline partials
           that are defined directly in the templates.

           Each map contains the partials defined on that level.
           Any partial in any of the maps can be accessed.
           When the level is out of scope, its map is removed.

         */
        std::vector<detail::partials_view_map> inlinePartials;

        /* A vector of partial block contents.

           Keeps all partial blocks, so they can be rendered
           at deeper levels when needed.

           If no partial block content is provided for
           any higher level partial, and a partial attempts to
           render {{@partial-block}}, we should return an error
           "The partial @partial-block could not be found".

           What's tricky is if a nested partial renders
           {{> @partial-block}}, and this partial block
           includes another {{> @partial-block}}, the second
           call should render the partial content of the
           outer partial and not recursively render the
           inner partial block.

           This is achieved by keeping all partial blocks
           in this vector and the partialBlockLevel index
           to indicate the current level of partial blocks.
           This level is usually partialBlocks.size(),
           and is decreased when we recursively render
           partial blocks at deeper levels so that they
           can potentially only use partial blocks from
           outer levels instead of always taking the
           last element of partialBlocks.back().

         */
        std::vector<std::string_view> partialBlocks;

        /* The current level of partial blocks.

           See `partialBlocks` for more information.

         */
        std::size_t partialBlockLevel = 0;

        /* The original context object used in the template.

           This allows us to always access the initial
           context via @root.

           This assumes the context is always an object
           because the state at deeper levels always
           use objects.

           If the root context is a dom::Value,
           it's available from `rootContext`.

         */
        dom::Object context;

        /* The root context value.

           The root context as a value, when applicable.

           In this case, {{.}} can be used to access the
           root context.

         */
        dom::Value rootContext;

        /* A stack of data objects.

           This stack is used to keep track of the
           context as we render the template at deeper
           levels.

           Elements are always taken from the highest level,
           and elements from lower levels can be accessed
           via "..".

         */
        std::vector<dom::Object> contextStack;

        // A stack of parent context values.
        std::vector<dom::Value> parentContext;

        /* The block values object used in the template.

           Block values can also be accessed from a block,
           and they take precedence over the usual data
           context.

         */
        dom::Object blockValues;

        // The block value paths object used in the template.
        dom::Object blockValuePaths;
    };
}

static bool
isCurrentContextSegment(std::string_view path)
{
    return path == "." || path == "this";
}

static bool
isIdChar(char c)
{
    // Identifiers may be any unicode character except for the following:
    // Whitespace ! " # % & ' ( ) * + , . / ; < = > @ [ \ ] ^ ` { | } ~
    static constexpr std::array<char, 32> invalidChars = {
        ' ', '!', '"', '#', '%', '&', '\'', '(', ')', '*', '+', ',', '.', '/',
        ';', '<', '=', '>', '@', '[', '\\', ']', '^', '`', '{', '|', '}', '~',
        '\t', '\r', '\n', '\0'};
    return !std::ranges::any_of(invalidChars, [c](char invalid) { return c == invalid; });
}

static std::string_view
popFirstSegment(std::string_view& path0)
{
    // ==============================================================
    // Skip dot segments
    // ==============================================================
    std::string_view path = path0;
    while (path.starts_with("./") || path.starts_with("[.]/") || path.starts_with("[.]."))
    {
        path.remove_prefix(path.front() == '.' ? 2 : 4);
    }

    // ==============================================================
    // Single dot segment
    // ==============================================================
    if (path == "." || path == "[.]")
    {
        path0 = {};
        return {};
    }

    // ==============================================================
    // Literal segment [...]
    // ==============================================================
    if (path.starts_with('['))
    {
        auto pos = path.find_first_of(']');
        if (pos == std::string_view::npos)
        {
            // '[' segment was never closed
            path0 = {};
            return {};
        }
        std::string_view seg = path.substr(0, pos + 1);
        path = path.substr(pos + 1);
        if (path.empty())
        {
            // rest of the path is empty, so this is the last segment
            path0 = path;
            return seg;
        }
        if (path.front() != '.' && path.front() != '/')
        {
            // segment has no valid continuation, so it's invalid
            path0 = path;
            return {};
        }
        path0 = path.substr(1);
        return seg;
    }

    // ==============================================================
    // Literal number segment
    // ==============================================================
    // In a literal number segment the dots are part of the segment
    if (
        std::ranges::all_of(path, [](char c) { return c == '.' || std::isdigit(c) != 0; }) &&
        std::ranges::count(path, '.') < 2)
    {
        // Number segment
        path0 = {};
        return path;
    }

    // ==============================================================
    // Dotdot segment
    // ==============================================================
    // If path starts with dotdot segment, the delimiter needs to be a slash
    if (path.starts_with("../"))
    {
        path0 = path.substr(3);
        return path.substr(0, 2);
    }
    if (path == "..")
    {
        path0 = {};
        return path;
    }

    // ==============================================================
    // Regular ID
    // ==============================================================
    auto it = std::ranges::find_if_not(path, isIdChar);
    auto pos = static_cast<std::size_t>(it - path.begin());
    bool endsAtDelimiter = it != path.end() && (*it == '.' || *it == '/');
    path0 = path.substr(pos + static_cast<std::size_t>(endsAtDelimiter));
    return path.substr(0, pos);
}

struct position_in_text
{
    std::size_t line = static_cast<std::size_t>(-1);
    std::size_t column = static_cast<std::size_t>(-1);
    std::size_t pos = static_cast<std::size_t>(-1);

    constexpr
    operator bool() const
    {
        return line != static_cast<std::size_t>(-1);
    }
};

static constexpr
position_in_text
find_position_in_text(
    std::string_view text,
    std::string_view substr)
{
    position_in_text res;
    if ((substr.data() >= text.data()) &&
        (substr.data() <= (text.data() + text.size())))
    {
        res.pos = static_cast<std::size_t>(substr.data() - text.data());
        res.line = static_cast<std::size_t>(
            std::ranges::count(text.substr(0, res.pos), '\n') + 1);
        if (res.line == 1)
        {
            res.column = res.pos;
        }
        else
        {
            res.column = res.pos - text.rfind('\n', res.pos) - 1;
        }
    }
    return res;
}

[[nodiscard]]
static
Expected<void, Error>
checkPath(std::string_view path0, detail::RenderState const& state)
{
    std::string_view path = path0;
    if (path.starts_with('@')) {
        path.remove_prefix(1);
    }
    std::string_view seg = popFirstSegment(path);
    bool areDotDots = seg == "..";
    seg = popFirstSegment(path);
    while (!seg.empty())
    {
        bool isDotDot = seg == "..";
        bool invalidPath =
            (!areDotDots && isDotDot) ||
            isCurrentContextSegment(seg);
        areDotDots = areDotDots && isDotDot;
        if (invalidPath)
        {
            std::string msg =
                "Invalid path: " +
                std::string(path0.substr(0, seg.data() + seg.size() - path0.data()));
            auto res = find_position_in_text(state.rootTemplateText, path0);
            if (res)
            {
                return Unexpected(
                    Error(msg, res.line, res.column, res.pos));
            }
            return Unexpected(Error(msg));
        }
        seg = popFirstSegment(path);
    }
    return {};
}

static std::pair<dom::Value, bool>
lookupPropertyImpl(
    dom::Object const& context,
    std::string_view path,
    detail::RenderState const& state,
    HandlebarsOptions const& opt)
{
    // Get first value from Object
    std::string_view segment = popFirstSegment(path);
    bool isLiteral = segment.starts_with('[') && segment.ends_with(']');
    std::string_view literalSegment = segment.substr(
        1 * static_cast<std::size_t>(isLiteral),
        segment.size() - (2 * static_cast<std::size_t>(isLiteral)));
    dom::Value cur = nullptr;
    if (isCurrentContextSegment(segment))
    {
        cur = context;
    }
    else if (!context.exists(literalSegment))
    {
        if (opt.strict || (opt.assumeObjects && !path.empty()))
        {
          std::string msg = std::format("\"{}\" not defined in {}",
                                        literalSegment, toString(context));
          auto res =
              find_position_in_text(state.rootTemplateText, literalSegment);
          if (res) {
            throw Error(msg, res.line, res.column, res.pos);
          }
            throw Error(msg);
        }
        else
        {
            return {dom::Kind::Undefined, false};
        }
    }
    else
    {
        cur = context.get(literalSegment);
    }

    // Recursively get more values from current value
    segment = popFirstSegment(path);
    isLiteral = segment.starts_with('[') && segment.ends_with(']');
    literalSegment = segment.substr(
        1 * static_cast<std::size_t>(isLiteral),
        segment.size() - (2 * static_cast<std::size_t>(isLiteral)));
    while (!literalSegment.empty())
    {
        // If current value is an Object, get the next value from it
        if (cur.isObject())
        {
            auto obj = cur.getObject();
            if (obj.exists(literalSegment))
            {
                cur = obj.get(literalSegment);
            }
            else
            {
                if (opt.strict)
                {
                  std::string msg = std::format("\"{}\" not defined in {}",
                                                literalSegment, toString(cur));
                  auto res = find_position_in_text(state.rootTemplateText,
                                                   literalSegment);
                  if (res) {
                    throw Error(msg, res.line, res.column, res.pos);
                  }
                    throw Error(msg);
                }
                else
                {
                    return {dom::Kind::Undefined, false};
                }
            }
        }
        // If current value is an Array, get the next value the stripped index
        else if (cur.isArray())
        {
            size_t index = 0;
            std::from_chars_result res = std::from_chars(
                literalSegment.data(),
                literalSegment.data() + literalSegment.size(),
                index);
            if (res.ec != std::errc())
            {
                return {nullptr, false};
            }
            auto& arr = cur.getArray();
            if (index >= arr.size())
            {
                return {nullptr, false};
            }
            cur = arr.at(index);
        }
        else
        {
            // Current value is not an Object or Array, so we can't get any more
            // segments from it
            return {dom::Kind::Undefined, false};
        }
        // Consume more segments to get into the array element
        segment = popFirstSegment(path);
        isLiteral = segment.starts_with('[') && segment.ends_with(']');
        literalSegment = segment.substr(
            1 * static_cast<std::size_t>(isLiteral),
            segment.size() - (2 * static_cast<std::size_t>(isLiteral)));
    }
    return {cur, true};
}

[[nodiscard]]
static
Expected<std::pair<dom::Value, bool>, Error>
lookupPropertyImpl(
    dom::Value const& context,
    std::string_view path,
    detail::RenderState const& state,
    HandlebarsOptions const& opt)
{
    using Res = std::pair<dom::Value, bool>;
    { auto _hbs_try1 = checkPath(path, state); if (!_hbs_try1) return Unexpected(_hbs_try1.error()); }

    // ==============================================================
    // "." / "this"
    // ==============================================================
    if (isCurrentContextSegment(path) || path.empty())
    {
        return Res{context, true};
    }
    // ==============================================================
    // Non-object key
    // ==============================================================
    if (context.kind() != dom::Kind::Object) {
        if (opt.strict || opt.assumeObjects)
        {
          std::string msg =
              std::format("\"{}\" not defined in {}", path, context);
          auto res = find_position_in_text(state.rootTemplateText, path);
          if (res) {
            return Unexpected(
                Error(msg, res.line, res.column, res.pos));
          }
            return Unexpected(Error(msg));
        }
        return Res{nullptr, false};
    }
    // ==============================================================
    // Object path
    // ==============================================================
    return lookupPropertyImpl(context.getObject(), path, state, opt);
}

template <std::convertible_to<std::string_view> S>
static Expected<std::pair<dom::Value, bool>, Error>
lookupPropertyImpl(
    dom::Value const& data,
    S const& path,
    detail::RenderState const& state,
    HandlebarsOptions const& opt)
{
    return lookupPropertyImpl(
        data, static_cast<std::string_view>(path), state, opt);
}

[[nodiscard]]
Expected<std::pair<dom::Value, bool>, Error>
lookupPropertyImpl(
    dom::Value const& context,
    dom::Value const& path,
    detail::RenderState const& state,
    HandlebarsOptions const& opt)
{
    using Res = std::pair<dom::Value, bool>;
    if (path.isString())
    {
        return lookupPropertyImpl(context, path.getString(), state, opt);
    }
    if (path.isInteger())
    {
        if (context.isArray())
        {
            auto& arr = context.getArray();
            if (path.getInteger() >= static_cast<std::int64_t>(arr.size()))
            {
                return Res{nullptr, false};
            }
            return Res{arr.at(static_cast<std::size_t>(path.getInteger())), true};
        }
        return lookupPropertyImpl(context, std::to_string(path.getInteger()), state, opt);
    }
    return Res{nullptr, false};
}

// ==============================================================
// Engine
// ==============================================================

struct defaultLogger {
    static constexpr std::array<std::string_view, 4> methodMap =
        {"debug", "info", "warn", "error"};
    std::int64_t level_ = 1;

    void
    operator()(dom::Array const& args) const {
        dom::Value level = lookupLevel(args.at(0));
        if (!level.isInteger() || level.getInteger() > level_) {
            return;
        }
        std::string_view method = methodMap[
            static_cast<std::size_t>(level.getInteger())];
        std::string out;
        OutputRef os(out);
        os << '[';
        os << method;
        os << ']';
        std::size_t const n = args.size();
        for (std::size_t i = 1; i < n; ++i) {
            HandlebarsOptions opt;
            opt.noEscape = true;
            format_to(os, args.at(i), opt);
            os << " ";
        }
        std::println("{}", out);
    }

    dom::Value
    lookupLevel(dom::Value level) const {
        if (level.isString()) {
            // Find level from map
            std::string levelStr(level.getString());
            // convert level string to lowercase
            auto toLower = [](char c) {
                int constexpr diff = 'A' - 'a';
                bool const isLower = (c >= 'a' && c <= 'z');
                return c + diff * isLower;
            };
            std::ranges::transform(levelStr, levelStr.begin(), toLower);
            auto levelMap = std::ranges::find(methodMap, levelStr);
            if (levelMap != methodMap.end()) {
                return levelMap - methodMap.begin();
            }

            // Find level as integer string
            int levelInt = 0;
            auto res = std::from_chars(
                levelStr.data(),
                levelStr.data() + levelStr.size(),
                levelInt);
            if (res.ec == std::errc()) {
                return levelInt;
            }
        }
        return level;
    }
};

Handlebars::Handlebars() {
    helpers::registerBuiltinHelpers(*this);
    registerLogger(dom::makeVariadicInvocable(defaultLogger{}));
}

// Find the next handlebars tag
// Returns true if found, false if not found.
// If found, tag is set to the tag text.
bool
findTag(std::string_view &tag, std::string_view templateText)
{
    if (templateText.size() < 4)
    {
        return false;
    }

    // Find opening tag
    auto pos = templateText.find("{{");
    if (pos == std::string_view::npos)
    {
        return false;
    }

    // Find closing tag
    std::string_view closeTagToken = "}}";
    std::string_view closeTagToken2;
    if (templateText.substr(pos).starts_with("{{!--"))
    {
        closeTagToken = "--}}";
        closeTagToken2 = "--~}}";
    }
    else if (templateText.substr(pos).starts_with("{{{{"))
    {
        closeTagToken = "}}}}";
    }
    else if (templateText.substr(pos).starts_with("{{{"))
    {
        closeTagToken = "}}}";
    }
    auto end = templateText.find(closeTagToken, pos);
    if (end == std::string_view::npos)
    {
        if (closeTagToken2.empty())
        {
            return false;
        }
        closeTagToken = closeTagToken2;
        end = templateText.find(closeTagToken, pos);
        if (end == std::string_view::npos)
        {
            return false;
        }
    }

    // Found tag
    tag = templateText.substr(pos, end - pos + closeTagToken.size());

    // Check if tag is escaped verbatim
    bool const escaped = pos != 0 && templateText[pos - 1] == '\\';
    if (escaped)
    {
        bool const doubleEscaped = pos != 1 && templateText[pos - 2] == '\\';
        tag = {tag.data() - 1 - doubleEscaped, tag.data() + tag.size()};
    }
    return true;
}

struct Handlebars::Tag {
    std::string_view buffer;

    // Tag type
    char type = '\0';

    // Secondary tag type
    char type2 = '\0';

    // From after type until closing tag
    std::string_view content;

    // First expression in content if more than one expression
    std::string_view helper;

    // Other expressions in content
    std::string_view arguments;

    // Block parameters
    std::string_view blockParams;

    // Whether to escape the result
    bool forceNoEscape{false};

    // Whether to escape the result
    bool rawBlock{false};

    // Whether to remove leading whitespace
    bool removeLWhitespace{false};

    // Whether to remove trailing whitespace
    bool removeRWhitespace{false};

    // Whether the whole tag content is escaped
    bool escaped{false};

    // Tag is standalone in its context
    bool isStandalone{false};

    // Standalone tag indent
    std::size_t standaloneIndent{0};
};

// Find next expression in tag content
bool
findExpr(
    std::string_view & expr,
    std::string_view arguments,
    bool allowKeyValue = true)
{
    // ==============================================================
    // Empty arguments
    // ==============================================================
    arguments = trim_spaces(arguments);
    if (arguments.empty()) {
        expr = arguments;
        return false;
    }

    // ==============================================================
    // Literal strings
    // ==============================================================
    for (auto quote: {'\"', '\''})
    {
        if (arguments.front() == quote)
        {
            auto close_pos = arguments.find(quote, 1);
            while (
                close_pos != std::string_view::npos &&
                arguments[close_pos - 1] == '\\')
            {
                // Skip escaped quote
                close_pos = arguments.find(quote, close_pos + 1);
            }
            if (close_pos == std::string_view::npos)
            {
                // No closing quote found, invalid expression
                return false;
            }
            expr = arguments.substr(0, close_pos + 1);
            return true;
        }
    }

    // ==============================================================
    // Subexpressions
    // ==============================================================
    if (arguments.front() == '(')
    {
        std::string_view all = arguments.substr(1);
        std::string_view sub;
        while (findExpr(sub, all)) {
            all.remove_prefix(sub.data() + sub.size() - all.data());
        }
        if (!all.starts_with(')'))
            return false;
        expr = arguments.substr(0, sub.data() + sub.size() - arguments.data() + 1);
        return true;
    }

    // ==============================================================
    // Key=value pair
    // ==============================================================
    if (allowKeyValue)
    {
        auto it = std::ranges::find_if_not(arguments, isIdChar);
        if (it != arguments.end() && *it == '=')
        {
            std::string_view value = arguments.substr(it - arguments.begin() + 1);
            if (findExpr(expr, value, false))
            {
                expr = arguments.substr(0, expr.data() + expr.size() - arguments.data());
                return true;
            }
        }
    }

    // ==============================================================
    // Path segments
    // ==============================================================
    // Pop path segments while we can with popFirstSegment(...)
    std::string_view arguments0 = arguments;
    if (arguments.starts_with('@'))
    {
        arguments.remove_prefix(1);
    }
    std::string_view seg = popFirstSegment(arguments);
    while (!seg.empty())
    {
        seg = popFirstSegment(arguments);
    }
    expr = arguments0.substr(0, arguments.data() - arguments0.data());
    return !expr.empty();
}


// Parse a tag into helper, expression and content
Handlebars::Tag
parseTag(
    std::string_view tagStr,
    std::string_view context)
{
    assert(tagStr.size() >= 4);
    Handlebars::Tag t;
    t.escaped = tagStr.front() == '\\';
    assert(tagStr[0 + t.escaped] == '{');
    assert(tagStr[1 + t.escaped] == '{');
    assert(tagStr[tagStr.size() - 1] == '}');
    assert(tagStr[tagStr.size() - 2] == '}');
    t.buffer = tagStr;
    tagStr = tagStr.substr(2 + t.escaped, tagStr.size() - 4 - t.escaped);

    // ==============================================================
    // No HTML escape {{{ ... }}}
    // ==============================================================
    t.forceNoEscape = false;
    if (!tagStr.empty() && tagStr.front() == '{' && tagStr.back() == '}')
    {
        t.forceNoEscape = true;
        tagStr = tagStr.substr(1, tagStr.size() - 2);
        if (!tagStr.empty() && tagStr.front() == '{' && tagStr.back() == '}')
        {
            t.rawBlock = true;
            tagStr = tagStr.substr(1, tagStr.size() - 2);
        }
    }

    // ==============================================================
    // Escaped tag \\{{ ... }}
    // ==============================================================
    if (t.escaped) {
        // Just get the content of expression is escaped
        t.content = tagStr;
        t.arguments = tagStr;
        return t;
    }

    // Remove whitespaces once again to support tags with extra whitespaces.
    // This makes invalid tags like "{{ #if condition }}" work instead of failing.
    tagStr = trim_spaces(tagStr);

    // ==============================================================
    // Whitespace control
    // ==============================================================
    if (tagStr.starts_with('~')) {
        // {{~ ... }}
        t.removeLWhitespace = true;
        tagStr.remove_prefix(1);
        tagStr = trim_spaces(tagStr);
    }
    if (tagStr.ends_with('~')) {
        // {{ ... ~}}
        t.removeRWhitespace = true;
        tagStr.remove_suffix(1);
        tagStr = trim_spaces(tagStr);
    }

    // Force no HTML escape after whitespace removal
    if (!tagStr.empty() && tagStr.front() == '{' && tagStr.back() == '}')
    {
        // {{~{ ... }~}}
        t.forceNoEscape = true;
        tagStr = tagStr.substr(1, tagStr.size() - 2);
        if (!tagStr.empty() && tagStr.front() == '{' && tagStr.back() == '}')
        {
            t.rawBlock = true;
            tagStr = tagStr.substr(1, tagStr.size() - 2);
        }
    }

    // ==============================================================
    // Empty tags
    // ==============================================================
    if (tagStr.empty())
    {
        t.type = ' ';
        t.content = tagStr.substr(0);
        t.helper = tagStr.substr(0);
        t.content = tagStr.substr(0);
        t.arguments = tagStr.substr(0);
        return t;
    }

    // ==============================================================
    // Unescaped with '&' {{& ... }}
    // ==============================================================
    // '&' is also used to unescape expressions
    if (tagStr.front() == '&') {
        t.forceNoEscape = true;
        tagStr.remove_prefix(1);
        tagStr = trim_spaces(tagStr);
    }

    // ==============================================================
    // Tag type {{# ... }}, {{/ ... }}, {{^ ... }}, {{> ... }}, {{! ... }}
    // ==============================================================
    if (tagStr.starts_with('^')) {
        t.type = '^';
        t.type2 = '^';
        tagStr.remove_prefix(1);
        tagStr = trim_spaces(tagStr);
        t.content = tagStr;
    } else if (tagStr.starts_with("else")) {
        t.type = '^';
        t.type2 = 'e';
        tagStr.remove_prefix(4);
        tagStr = trim_spaces(tagStr);
        t.content = tagStr;
    }
    else
    {
        static constexpr std::array<char, 5> tag_types({'#', '/', '>', '!'});
        auto it = std::ranges::find_if(tag_types, [tagStr](char c) { return c == tagStr.front(); });
        if (it != tag_types.end()) {
            t.type = tagStr.front();
            tagStr.remove_prefix(1);
            if (t.type == '#') {
                if (tagStr.starts_with('>')) {
                    // Partial block. # is a secondary tag
                    t.type = '>';
                    t.type2 = '#';
                    tagStr.remove_prefix(1);
                } else if (tagStr.starts_with('*')) {
                    // Partial block. # is a secondary tag
                    t.type = '*';
                    t.type2 = '#';
                    tagStr.remove_prefix(1);
                }
            }
            tagStr = trim_spaces(tagStr);
        } else if (t.rawBlock) {
            t.type = '#';
        }
        else
        {
            t.type = ' ';
        }
        t.content = tagStr;
    }

    // ==============================================================
    // Block parameters {{# ... as | ... |}}
    // ==============================================================
    if (tagStr.ends_with('|')) {
        auto blockStart = tagStr.find_last_of('|', tagStr.size() - 2);
        if (blockStart != std::string_view::npos) {
            std::string_view tagStr1 = tagStr;
            tagStr1.remove_suffix(tagStr.size() - blockStart);
            tagStr1 = trim_rspaces(tagStr1);
            if (tagStr1.ends_with(" as")) {
                t.blockParams = trim_spaces(tagStr.substr(blockStart));
                t.blockParams.remove_prefix(1);
                t.blockParams.remove_suffix(1);
                tagStr = tagStr1.substr(0, tagStr1.size() - 3);
            }
        }
    }

    // ==============================================================
    // Helper and arguments {{ helper arg... }}
    // ==============================================================
    std::string_view expr;
    if (findExpr(expr, tagStr)) {
        t.helper = expr;
        tagStr.remove_prefix(expr.data() + expr.size() - tagStr.data());
        t.arguments = trim_spaces(tagStr);
    }
    else
    {
        t.helper = tagStr;
        t.arguments = {};
    }

    // ==============================================================
    // Check if tag is standalone
    // ==============================================================
    static constexpr std::array<char, 6> standalone_tag_types({'#', '^', '/', '>', '*', '!'});
    bool const checkStandalone = std::ranges::find(
        standalone_tag_types, t.type) != standalone_tag_types.end();
    if (checkStandalone)
    {
        assert(t.buffer.data() >= context.data());
        assert(t.buffer.data() + t.buffer.size() <= context.data() + context.size());

        // Check if tag is standalone
        std::string_view beforeTag = context.substr(
            0, t.buffer.data() - context.data());
        auto posL = beforeTag.find_last_not_of(' ');
        bool isStandaloneL =
            posL == std::string_view::npos || beforeTag[posL] == '\n';
        if (!isStandaloneL && posL != 0)
        {
            isStandaloneL = beforeTag[posL - 1] == '\r' && beforeTag[posL] == '\n';
        }
        std::string_view afterTag = context.substr(
            t.buffer.data() + t.buffer.size() - context.data());
        auto posR = afterTag.find_first_not_of(' ');
        bool isStandaloneR =
            posR == std::string_view::npos || afterTag[posR] == '\n';
        if (!isStandaloneR && posR != afterTag.size() - 1)
        {
            isStandaloneR = afterTag[posR] == '\r' && afterTag[posR + 1] == '\n';
        }

        t.isStandalone = isStandaloneL && isStandaloneR;

        // Get standalone indent
        std::string_view lastLine = beforeTag;
        if (posL != std::string_view::npos)
        {
            lastLine = beforeTag.substr(posL + 1);
        }
        t.standaloneIndent = t.isStandalone ? lastLine.size() : 0;
    }
    return t;
}

Expected<void, Error>
Handlebars::
try_render_to(
    OutputRef& out,
    std::string_view templateText,
    dom::Value const& context,
    HandlebarsOptions const& options) const
{
    detail::RenderState state;
    state.rootTemplateText = templateText;
    state.templateText = templateText;
    if (options.data.isObject()) {
        state.context = options.data.getObject();
    }
    state.inlinePartials.emplace_back();
    state.rootContext = context;
    state.contextStack.emplace_back(state.context);
    return try_render_to_impl(out, context, options, state);
}

Expected<void, Error>
Handlebars::
try_render_to_impl(
    OutputRef& out,
    dom::Value const& context,
    HandlebarsOptions const& opt,
    detail::RenderState& state) const
{
    while (!state.templateText.empty())
    {
        // ==============================================================
        // Find next tag
        // ==============================================================
        std::string_view tagStr;
        if (!findTag(tagStr, state.templateText))
        {
            out << state.templateText;
            break;
        }
        bool const isDoubleEscaped = tagStr.starts_with("\\\\");
        if (isDoubleEscaped) {
            tagStr.remove_prefix(2);
        }
        std::size_t tagStartPos = tagStr.data() - state.templateText.data();
        Tag tag = parseTag(tagStr, state.rootTemplateText);

        // ==============================================================
        // Render template text before tag
        // ==============================================================
        std::string_view beforeTag = state.templateText.substr(0, tagStartPos - isDoubleEscaped);
        if (tag.removeLWhitespace) {
            beforeTag = trim_rspaces(beforeTag);
        }
        else if (!opt.ignoreStandalone && tag.isStandalone)
        {
            if (tag.type == '#' || tag.type == '^' || tag.type == '/' || tag.type == '!')
            {
                beforeTag = trim_rdelimiters(beforeTag, " ");
            }
        }
        out << beforeTag;

        // ==============================================================
        // Render escaped tag
        // ==============================================================
        state.templateText.remove_prefix(tagStartPos + tagStr.size());
        if (tag.escaped)
        {
            out << tag.buffer.substr(1);
            continue;
        }

        // ==============================================================
        // Render tag
        // ==============================================================
        { auto _hbs_try2 = renderTag(tag, out, context, opt, state); if (!_hbs_try2) return Unexpected(_hbs_try2.error()); }

        // ==============================================================
        // Advance template text
        // ==============================================================
        if (tag.removeRWhitespace && tag.type != '#')
        {
            state.templateText = trim_lspaces(state.templateText);
        }
    }
    return {};
}

namespace {
constexpr
bool
is_literal_value(
    std::string_view expression,
    std::string_view value)
{
    if (expression == value)
        return true;
    if (expression.size() < value.size() + 2)
        return false;
    std::size_t open_pos = expression.size() - value.size() - 2;
    std::size_t close_pos = expression.size() - 1;
    std::string_view last_key = expression.substr(open_pos + 1, close_pos - open_pos - 1);
    if (expression[open_pos] == '[' && expression[close_pos] == ']' && last_key == value)
        return true;
    return false;
}

constexpr
bool
is_literal_string(std::string_view expression)
{
    if (expression.size() < 2)
        return false;
    return std::ranges::any_of(std::array{ '\"', '\''}, [&expression](char c) {
        return expression.front() == c && expression.back() == c;
    });
}

constexpr
bool
is_literal_integer(std::string_view expression)
{
    expression = trim_spaces(expression);
    if (expression.empty()) {
        return false;
    }
    if (expression.front() == '-' || expression.front() == '+') {
        expression = expression.substr(1);
    }
    return std::ranges::all_of(expression, [](char c) {
        return std::isdigit(c);
    });
}

constexpr
std::pair<std::string_view, std::string_view>
findKeyValuePair(std::string_view expression)
{
    if (expression.empty() ||
        (expression.front() == '(' && expression.back() == ')'))
        return { {}, {} };
    auto pos = expression.find('=');
    if (pos == std::string_view::npos)
        return { {}, {} };
    std::string_view key = expression.substr(0, pos);
    std::string_view value = expression.substr(pos + 1);
    if (!key.empty() && !value.empty() && key.front() != '\'' && key.front() != '\"')
        return { key, value };
    return { {}, {} };
}

constexpr
char
unescapeChar(char c)
{
    switch (c) {
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'v': return '\v';
    case '0': return '\0';
    case '\\': return '\\';
    case '\'': return '\'';
    case '\"': return '\"';
    default: return static_cast<char>(-1);
    }
}

std::string
unescapeString(std::string_view str) {
    std::string unescapedString;
    if (str.empty()) {
        return unescapedString;
    }
    if (str.front() == '\"' || str.front() == '\'') {
        str.remove_prefix(1);
    }
    if (str.back() == '\"' || str.back() == '\'') {
        str.remove_suffix(1);
    }
    unescapedString.reserve(str.length());
    for (std::size_t i = 0; i < str.length(); ++i) {
        if (str[i] != '\\') {
            unescapedString.push_back(str[i]);
        }
        else
        {
            if (i + 1 < str.length()) {
                char c = unescapeChar(str[i + 1]);
                if (c == static_cast<char>(-1)) {
                    unescapedString.push_back('\\');
                    unescapedString.push_back(str[i + 1]);
                }
                else
                {
                    unescapedString.push_back(c);
                }
                ++i;
            }
            else
            {
                unescapedString.push_back('\\');
            }
        }
    }
    return unescapedString;
}

// detail::appendContextPath now lives inline in <mrdocs/Handlebars/detail/Engine.hpp>
// so the helper translation units can share it.
}

struct HbsHelperObjectImpl
    : public dom::ObjectImpl
{
    dom::Value name_;
    dom::Value context_;
    dom::Value data_;
    dom::Value log_;
    dom::Value hash_;
    dom::Value ids_;
    dom::Value hashIds_;
    dom::Value lookupProperty_;
    dom::Value blockParams_;
    dom::Value write_;
    dom::Value fn_;
    dom::Value inverse_;
    dom::Value write_inverse_;
    dom::Object overlay_;

public:
    ~HbsHelperObjectImpl() override = default;

    char const*
    type_key() const noexcept override
    {
        return "handlebarsHelperObject";
    }

    std::size_t size() const override
    {
        return 13 + overlay_.size();
    }

    dom::Value get(std::string_view key) const override
    {
        if (key == "name") return name_;
        if (key == "context") return context_;
        if (key == "data") return data_;
        if (key == "log") return log_;
        if (key == "hash") return hash_;
        if (key == "ids") return ids_;
        if (key == "hashIds") return hashIds_;
        if (key == "lookupProperty") return lookupProperty_;
        if (key == "blockParams") return blockParams_;
        if (key == "write") return write_;
        if (key == "fn") return fn_;
        if (key == "inverse") return inverse_;
        if (key == "write_inverse") return write_inverse_;
        return overlay_.get(key);
    }

    void set(dom::String key, dom::Value value) override
    {
        if (key == "name") { name_ = value; return; }
        if (key == "context") { context_ = value; return; }
        if (key == "data") { data_ = value; return; }
        if (key == "log") { log_ = value; return; }
        if (key == "hash") { hash_ = value; return; }
        if (key == "ids") { ids_ = value; return; }
        if (key == "hashIds") { hashIds_ = value; return; }
        if (key == "lookupProperty") { lookupProperty_ = value; return; }
        if (key == "blockParams") { blockParams_ = value; return; }
        if (key == "write") { write_ = value; return; }
        if (key == "fn") { fn_ = value; return; }
        if (key == "inverse") { inverse_ = value; return; }
        if (key == "write_inverse") { write_inverse_ = value; return; }
        overlay_.set(key, value);
    }

    bool visit(std::function<bool(dom::String, dom::Value)> visitor) const override
    {
        if (!visitor("name", name_)) return false;
        if (!visitor("context", context_)) return false;
        if (!visitor("data", data_)) return false;
        if (!visitor("log", log_)) return false;
        if (!visitor("hash", hash_)) return false;
        if (!visitor("ids", ids_)) return false;
        if (!visitor("hashIds", hashIds_)) return false;
        if (!visitor("lookupProperty", lookupProperty_)) return false;
        if (!visitor("blockParams", blockParams_)) return false;
        if (!visitor("write", write_)) return false;
        if (!visitor("fn", fn_)) return false;
        if (!visitor("inverse", inverse_)) return false;
        if (!visitor("write_inverse", write_inverse_)) return false;
        return overlay_.visit(visitor);
    }

    bool exists(std::string_view key) const override
    {
        if (key == "name") return true;
        if (key == "context") return true;
        if (key == "data") return true;
        if (key == "log") return true;
        if (key == "hash") return true;
        if (key == "ids") return true;
        if (key == "hashIds") return true;
        if (key == "lookupProperty") return true;
        if (key == "blockParams") return true;
        if (key == "write") return true;
        if (key == "fn") return true;
        if (key == "inverse") return true;
        if (key == "write_inverse") return true;
        return overlay_.exists(key);
    }
};

Expected<Handlebars::evalExprResult, Error>
Handlebars::
evalExpr(
    dom::Value const& context,
    std::string_view expression,
    detail::RenderState& state,
    HandlebarsOptions const& opt,
    bool evalLiterals) const
{
    using Res = Handlebars::evalExprResult;
    if (evalLiterals)
    {
        // ==============================================================
        // Literal values
        // ==============================================================
        if (is_literal_value(expression, "true"))
        {
            return Res{true, true, true};
        }
        if (is_literal_value(expression, "false"))
        {
            return Res{false, true, true};
        }
        if (is_literal_value(expression, "null"))
        {
            return Res{nullptr, true, true};
        }
        if (is_literal_value(expression, "undefined") || expression.empty())
        {
            return Res{dom::Kind::Undefined, true, true};
        }
        if (expression == "." || expression == "this")
        {
            return Res{context, true, false};
        }
        if (is_literal_string(expression))
        {
            return Res{unescapeString(expression), true, true};
        }
        if (is_literal_integer(expression))
        {
            std::int64_t value;
            auto res = std::from_chars(
                expression.data(),
                expression.data() + expression.size(),
                value);
            if (res.ec != std::errc())
            {
                return Res{std::int64_t(0), true, true};
            }
            return Res{value, true, true};
        }
        // ==============================================================
        // Subexpressions
        // ==============================================================
        if (expression.starts_with('(') && expression.ends_with(')'))
        {
            std::string_view all = expression.substr(1, expression.size() - 2);
            std::string_view helper;
            findExpr(helper, all);
            auto [fn, found] = getHelper(helper, false);
            if (!found)
            {
                auto res = find_position_in_text(state.rootTemplateText, helper);
                std::string msg(helper);
                msg += " is not a function";
                if (res)
                {
                    return Unexpected(Error(msg, res.line, res.column, res.pos));
                }
                return Unexpected(Error(msg));
            }
            all.remove_prefix(helper.data() + helper.size() - all.data());
            dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
            dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
            cb.set("name", helper);
            cb.set("context", context);
            dom::Object data;
            if (!state.context.empty())
            {
                data = createFrame(state.context);
            }
            data.set("root", state.rootContext);
            cb.set("data", data);
            cb.set("root", state.rootContext);
            cb.set("log", logger_);
            setupArgs(all, context, state, args, cb, opt);
            Expected<dom::Value> exp = fn.call(args);
            if (!exp)
            {
                Error e = exp.error();
                auto res = find_position_in_text(state.rootTemplateText, helper);
                std::string_view msg = e.message();
                if (res)
                {
                    return Unexpected(Error(msg, res.line, res.column, res.pos));
                }
                return Unexpected(Error(msg));
            }
            return Res{*exp, true, false, true};
        }
    }
    // ==============================================================
    // Private data
    // ==============================================================
    if (expression.starts_with('@'))
    {
        { auto _hbs_try3 = checkPath(expression, state); if (!_hbs_try3) return Unexpected(_hbs_try3.error()); }
        expression.remove_prefix(1);
        dom::Value data = state.context;
        if (expression == "root" || expression.starts_with("root.") || expression.starts_with("root/"))
        {
            popFirstSegment(expression);
            if (state.context.exists("root"))
            {
                data = state.context.get("root");
            }
            else
            {
                data = state.rootContext;
            }
        }
        else if (expression.starts_with("./") || expression.starts_with("../"))
        {
            auto rDataStack = std::ranges::views::reverse(state.contextStack);
            auto dataIt = rDataStack.begin();
            while (!expression.empty())
            {
                if (expression.starts_with("./"))
                {
                    expression.remove_prefix(2);
                    continue;
                }
                if (expression.starts_with("../"))
                {
                    expression.remove_prefix(3);
                    if (dataIt == rDataStack.end())
                    {
                        return Res{nullptr, false, false};
                    }
                    data = *dataIt;
                    ++dataIt;
                    continue;
                }
                break;
            }
        }
        auto _hbs_try4 = lookupPropertyImpl(data, expression, state, opt); if (!_hbs_try4) return Unexpected(_hbs_try4.error()); auto r = *std::move(_hbs_try4);
        auto [res, found] = r;
        return Res{res, found, false};
    }

    // ==============================================================
    // Dotdot context path
    // ==============================================================
    HandlebarsOptions noStrict = opt;
    noStrict.strict = false;
    noStrict.assumeObjects = false;
    if (expression.starts_with("..")) {
        // Get value from parent helper contexts
        std::size_t dotdots = 1;
        expression.remove_prefix(2);
        if (expression.starts_with('/')) {
            expression.remove_prefix(1);
        }
        while (expression.starts_with("..")) {
            ++dotdots;
            expression.remove_prefix(2);
            if (expression.starts_with('/')) {
                expression.remove_prefix(1);
            }
        }
        if (dotdots > state.parentContext.size()) {
            return Res{dom::Kind::Undefined, false};
        }
        dom::Value parentCtx =
            state.parentContext[state.parentContext.size() - dotdots];
        auto _hbs_try5 = lookupPropertyImpl(parentCtx, expression, state, noStrict); if (!_hbs_try5) return Unexpected(_hbs_try5.error()); auto r = *std::move(_hbs_try5);
        auto [res, found] = r;
        return Res{res, found, false};
    }
    // ==============================================================
    // Pathed type
    // ==============================================================
    // Precedence:
    // 1) Pathed context values
    // 2) Block values
    // 3) Context values
    bool isPathedValue = false;
    if (expression == "this" ||
        expression == "." ||
        expression.starts_with("this.") ||
        expression.starts_with("./"))
    {
        isPathedValue = true;
    }

    // ==============================================================
    // Pathed context values
    // ==============================================================
    dom::Value r;
    bool defined;
    if (isPathedValue)
    {
        auto _hbs_try6 = lookupPropertyImpl(context, expression, state, noStrict); if (!_hbs_try6) return Unexpected(_hbs_try6.error()); std::tie(r, defined) = *std::move(_hbs_try6);
        if (defined) {
            return Res{r, defined, false};
        }
    }

    // ==============================================================
    // Block values
    // ==============================================================
    std::tie(r, defined) = lookupPropertyImpl(state.blockValues, expression, state, noStrict);
    if (defined)
    {
        return Res{r, defined, false, false, true};
    }

    // ==============================================================
    // Literal object key
    // ==============================================================
    if (context.kind() == dom::Kind::Object) {
        auto& obj = context.getObject();
        if (obj.exists(expression))
        {
            return Res{obj.get(expression), true, false};
        }
    }

    // ==============================================================
    // Context values
    // ==============================================================
    HandlebarsOptions strictOpt = opt;
    strictOpt.strict = opt.strict && !opt.compat;
    strictOpt.assumeObjects = opt.assumeObjects && !opt.compat;
    auto _hbs_try7 = lookupPropertyImpl(context, expression, state, strictOpt); if (!_hbs_try7) return Unexpected(_hbs_try7.error()); std::tie(r, defined) = *std::move(_hbs_try7);
    if (defined) {
        return Res{r, defined, false};
    }

    // ==============================================================
    // Parent contexts
    // ==============================================================
    if (opt.compat)
    {
        // Dotted names should be resolved against former resolutions
        bool isDotted = isPathedValue;
        std::string_view firstSeg;
        if (!isDotted)
        {
            std::string_view expression0 = expression;
            firstSeg = popFirstSegment(expression);
            isDotted = !expression.empty();
            expression = expression0;
        }

        if (isDotted)
        {
            if (context.kind() == dom::Kind::Object)
            {
                // Context has first segment of dotted object.
                // -> Context has priority even if result is undefined.
                auto& obj = context.getObject();
                if (obj.exists(firstSeg))
                {
                    return Res{r, false, false};
                }
            }
        }

        // Find in parent contexts
        auto parentContexts = std::ranges::views::reverse(state.parentContext);
        for (auto const& parentContext: parentContexts)
        {
            auto _hbs_try8 = lookupPropertyImpl(parentContext, expression, state, noStrict); if (!_hbs_try8) return Unexpected(_hbs_try8.error()); std::tie(r, defined) = *std::move(_hbs_try8);
            if (defined)
            {
                return Res{r, defined, false};
            }
        }
    }

    if (opt.strict)
    {
      std::string msg = std::format("\"{}\" not defined", expression);
      return Unexpected(Error(msg));
    }
    return Res{dom::Kind::Undefined, false, false};
}

auto
Handlebars::
getHelper(std::string_view helper, bool isNoArgBlock) const
    -> std::pair<dom::Function, bool>
{
    auto it = helpers_.find(helper);
    if (it != helpers_.end())
    {
        return {it->second, true};
    }
    helper = !isNoArgBlock ? "helperMissing" : "blockHelperMissing";
    it = helpers_.find(helper);
    assert(it != helpers_.end());
    return {it->second, false};
}

auto
Handlebars::
getPartial(
    std::string_view name,
    detail::RenderState const& state) const
    -> std::pair<std::string_view, bool>
{
    // Inline partials
    auto blockPartials = std::ranges::views::reverse(state.inlinePartials);
    for (auto blockInlinePartials: blockPartials)
    {
        auto it = blockInlinePartials.find(name);
        if (it != blockInlinePartials.end())
        {
            return {it->second, true};
        }
    }

    // Main partials
    auto it = this->partials_.find(name);
    if (it != this->partials_.end())
    {
        return {it->second, true};
    }

    // Partial block
    if (name == "@partial-block" &&
        state.partialBlockLevel <= state.partialBlocks.size())
    {
        return {
            state.partialBlocks[state.partialBlockLevel - 1],
            true};
    }

    return { {}, false };
}

// Parse a block starting at templateText
Expected<void, Error>
parseBlock(
    std::string_view blockName,
    Handlebars::Tag const& tag,
    HandlebarsOptions const& opt,
    detail::RenderState const& state,
    std::string_view &templateText,
    OutputRef &out,
    std::string_view &fnBlock,
    std::string_view &inverseBlocks,
    Handlebars::Tag &inverseTag,
    bool isChainedBlock)
{
    // ==============================================================
    // Initial blocks
    // ==============================================================
    fnBlock = templateText;
    inverseBlocks = {};
    if (!opt.ignoreStandalone && tag.isStandalone)
    {
        fnBlock = trim_ldelimiters(fnBlock, " ");
        if (fnBlock.starts_with('\n'))
        {
            fnBlock.remove_prefix(1);
        }
        else if (fnBlock.starts_with("\r\n"))
        {
            fnBlock.remove_prefix(2);
        }
    }

    // ==============================================================
    // Iterate over the template to find tags and blocks
    // ==============================================================
    Handlebars::Tag closeTag;
    int l = 1;
    std::string_view* curBlock = &fnBlock;
    bool closed = false;
    while (!templateText.empty())
    {
        // ==============================================================
        // Find next tag
        // ==============================================================
        std::string_view tagStr;
        if (!findTag(tagStr, templateText))
        {
            break;
        }

        Handlebars::Tag curTag = parseTag(tagStr, state.rootTemplateText);

        // move template after the tag
        auto tag_pos = curTag.buffer.data() - templateText.data();
        templateText.remove_prefix(tag_pos + curTag.buffer.size());

        // ==============================================================
        // Update section level
        // ==============================================================
        if (!tag.rawBlock) {
            bool isRegularBlock = curTag.type == '#' || curTag.type2 == '#';
            // Sequential invert blocks {{^x}} are blocks nested inside the
            // current block, different from than a new "else" block.
            // {{^bool}}A{{^bool}}B{{/bool}}C{{/bool}} -> nested
            // {{^bool}}A{{else if bool}}B{{/bool}} -> not nested
            bool isNestedInvert =
                curTag.type == '^' && curTag.type2 == '^' && !curTag.content.empty();
            if (isRegularBlock || isNestedInvert) {
                // Opening a child section tag
                ++l;
            } else if (curTag.type == '/') {
                // Closing a section tag
                --l;
                if (l == 0) {
                    // ==============================================================
                    // Close main section tag
                    // ==============================================================
                    closeTag = curTag;
                    bool const isBlockNameMismatch = closeTag.content != blockName;
                    if (isBlockNameMismatch)
                    {
                        auto res = find_position_in_text(state.rootTemplateText, blockName);
                        std::string msg(blockName);
                        msg += " doesn't match ";
                        msg += closeTag.content;
                        if (res)
                        {
                            return Unexpected(Error(msg, res.line, res.column, res.pos));
                        }
                        return Unexpected(Error(msg));
                    }
                    closed = true;
                    *curBlock = {curBlock->data(), closeTag.buffer.data()};
                    if (closeTag.removeLWhitespace) {
                        *curBlock = trim_rspaces(*curBlock);
                    }
                    else if (!opt.ignoreStandalone && closeTag.isStandalone)
                    {
                        *curBlock = trim_rdelimiters(*curBlock, " ");
                    }
                    if (closeTag.removeRWhitespace) {
                        templateText = trim_lspaces(templateText);
                    }
                    break;
                }
            }

            // ==============================================================
            // Check chained block inversion
            // ==============================================================
            bool const isMainBlock = curBlock != &inverseBlocks;
            bool const isEndOfMainBlock = l == 1 && isMainBlock;
            if (isEndOfMainBlock) {
                if (curTag.type == '^') {
                    inverseTag = curTag;

                    // ==============================================================
                    // Finalize current block content
                    // ==============================================================
                    *curBlock = {curBlock->data(), curTag.buffer.data()};
                    if (inverseTag.removeLWhitespace) {
                        *curBlock = trim_rspaces(*curBlock);
                    }
                    if (tag.removeRWhitespace) {
                        *curBlock = trim_lspaces(*curBlock);
                    }

                    // ==============================================================
                    // Inverse current block
                    // ==============================================================
                    curBlock = &inverseBlocks;
                    *curBlock = templateText;
                    if (inverseTag.removeRWhitespace) {
                        *curBlock = trim_lspaces(*curBlock);
                        templateText = trim_lspaces(templateText);
                    }
                }
            }
        }
        else
        {
            // ==============================================================
            // Raw blocks
            // ==============================================================
            if (curTag.type == '/' && tag.rawBlock == curTag.rawBlock && blockName == curTag.content) {
                // Closing the raw section: l = 0;
                closed = true;
                closeTag = curTag;
                *curBlock = {curBlock->data(), closeTag.buffer.data()};
                if (closeTag.removeLWhitespace) {
                    *curBlock = trim_rspaces(*curBlock);
                }
                if (closeTag.removeRWhitespace) {
                    templateText = trim_lspaces(templateText);
                }
                break;
            }
        }
    }

    // ==============================================================
    // Check if block was closed
    // ==============================================================
    if (!closed && !isChainedBlock) {
        auto res = find_position_in_text(state.rootTemplateText, blockName);
        std::string msg(blockName);
        msg += " missing closing braces";
        if (res)
        {
            return Unexpected(Error(msg, res.line, res.column, res.pos));
        }
        return Unexpected(Error(msg));
    }

    // ==============================================================
    // Apply open tag whitespace control to block
    // ==============================================================
    if (tag.removeRWhitespace) {
        fnBlock = trim_lspaces(fnBlock);
    }

    // ==============================================================
    // Apply close tag whitespace control after block
    // ==============================================================
    if (closeTag.removeRWhitespace) {
        templateText = trim_lspaces(templateText);
    }
    else if (!opt.ignoreStandalone && closeTag.isStandalone)
    {
        templateText = trim_ldelimiters(templateText, " ");
        if (templateText.starts_with('\n'))
        {
            templateText.remove_prefix(1);
        }
        else if (templateText.starts_with("\r\n"))
        {
            templateText.remove_prefix(2);
        }
    }
    return {};
}


// Render a handlebars tag
Expected<void, Error>
Handlebars::
renderTag(
    Tag const& tag,
    OutputRef& out,
    dom::Value const& context,
    HandlebarsOptions const& opt,
    detail::RenderState& state) const {
    if ('#' == tag.type || '^' == tag.type)
    {
        return renderBlock(tag.helper, tag, out, context, opt, state, false);
    }
    else if ('>' == tag.type)
    {
        return renderPartial(tag, out, context, opt, state);
    }
    else if ('*' == tag.type)
    {
        return renderDecorator(tag, out, context, opt, state);
    }
    else if ('/' != tag.type && '!' != tag.type)
    {
        return renderExpression(tag, out, context, opt, state);
    }
    else if ('!' == tag.type)
    {
        // Remove whitespace around standalone comments
        if (!opt.ignoreStandalone && tag.isStandalone)
        {
            state.templateText = trim_ldelimiters(state.templateText, " ");
            if (state.templateText.starts_with('\n'))
            {
                state.templateText.remove_prefix(1);
            }
            else if (state.templateText.starts_with("\r\n"))
            {
                state.templateText.remove_prefix(2);
            }
        }
    }
    return {};
}

Expected<void, Error>
Handlebars::
renderExpression(
    Handlebars::Tag const &tag,
    OutputRef &out,
    dom::Value const & context,
    HandlebarsOptions const &opt,
    detail::RenderState& state) const
{
    if (tag.helper.empty())
    {
        return {};
    }

    auto opt2 = opt;
    opt2.noEscape = tag.forceNoEscape || opt.noEscape;

    // ==============================================================
    // Helpers as block params
    // ==============================================================
    if (state.blockValues.exists(tag.helper))
    {
        auto v = state.blockValues.get(tag.helper);
        format_to(out, v, opt2);
        if (tag.removeRWhitespace) {
            state.templateText = trim_lspaces(state.templateText);
        }
        return {};
    }

    // ==============================================================
    // Helper as function
    // ==============================================================
    auto it = helpers_.find(tag.helper);
    if (it != helpers_.end()) {
        auto fn = it->second;
        dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
        dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
        cb.set("name", tag.helper);
        cb.set("context", context);
        dom::Object data;
        if (!state.context.empty())
        {
            data = createFrame(state.context);
        }
        data.set("root", state.rootContext);
        cb.set("data", data);
        cb.set("log", logger_);
        HandlebarsOptions noStrict = opt;
        noStrict.strict = false;
        { auto _hbs_try9 = setupArgs(tag.arguments, context, state, args, cb, noStrict); if (!_hbs_try9) return Unexpected(_hbs_try9.error()); }
        Expected<dom::Value> exp = fn.call(args);
        if (!exp)
        {
            Error e = exp.error();
            auto res = find_position_in_text(state.rootTemplateText, tag.helper);
            std::string_view msg = e.message();
            if (res)
            {
                return Unexpected(Error(msg, res.line, res.column, res.pos));
            }
            return Unexpected(Error(msg));
        }
        dom::Value result = *exp;
        if (!result.isUndefined()) {
            opt2.noEscape = opt2.noEscape || result.isSafeString();
            format_to(out, result, opt2);
        }
        if (tag.removeRWhitespace) {
            state.templateText = trim_lspaces(state.templateText);
        }
        return {};
    }

    // ==============================================================
    // Helper as expression
    // ==============================================================
    std::string_view helper_expr = tag.helper;
    std::string unescaped;
    if (is_literal_string(tag.helper))
    {
        unescaped = unescapeString(helper_expr);
        helper_expr = unescaped;
    }
    auto _hbs_try10 = evalExpr(context, helper_expr, state, opt, false); if (!_hbs_try10) return Unexpected(_hbs_try10.error()); auto resV = *std::move(_hbs_try10);
    if (resV.found)
    {
        if (resV.value.isFunction())
        {
            dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
            dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
            cb.set("name", helper_expr);
            cb.set("context", context);
            dom::Object data;
            if (!state.context.empty())
            {
                data = createFrame(state.context);
            }
            data.set("root", state.rootContext);
            cb.set("data", data);
            cb.set("log", logger_);
            HandlebarsOptions noStrict = opt;
            noStrict.strict = false;
            setupArgs(tag.arguments, context, state, args, cb, noStrict);
            Expected<dom::Value> expV2 = resV.value.getFunction().call(args);
            if (!expV2) {
                Error e = expV2.error();
                auto res = find_position_in_text(state.rootTemplateText, helper_expr);
                std::string_view msg = e.message();
                if (res) {
                    return Unexpected(Error(msg, res.line, res.column, res.pos));
                }
                return Unexpected(Error(msg));
            }
            dom::Value v2 = *std::move(expV2);
            format_to(out, v2, opt2);
        }
        else
        {
            format_to(out, resV.value, opt2);
        }
        return {};
    }
    else if (opt.strict)
    {
      std::string msg = std::format("\"{}\" not defined in {}", helper_expr,
                                    toString(context));
      return Unexpected(Error(msg));
    }

    // ==============================================================
    // helperMissing hook
    // ==============================================================
    auto [fn, found] = getHelper(helper_expr, false);
    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
    cb.set("name", helper_expr);
    cb.set("context", context);
    dom::Object data;
    if (!state.context.empty())
    {
        data = createFrame(state.context);
    }
    data.set("root", state.rootContext);
    cb.set("data", data);
    cb.set("log", logger_);
    HandlebarsOptions noStrict = opt;
    noStrict.strict = false;
    setupArgs(tag.arguments, context, state, args, cb, noStrict);
    Expected<dom::Value> exp2 = fn.call(args);
    if (!exp2)
    {
        Error e = exp2.error();
        auto res = find_position_in_text(state.rootTemplateText, helper_expr);
        std::string_view msg = e.message();
        if (res)
        {
            return Unexpected(Error(msg, res.line, res.column, res.pos));
        }
        return Unexpected(Error(msg));
    }
    dom::Value res = *exp2;
    if (!res.isUndefined())
    {
        opt2.noEscape = opt2.noEscape || res.isSafeString();
        format_to(out, res, opt2);
    }
    if (tag.removeRWhitespace)
    {
        state.templateText = trim_lspaces(state.templateText);
    }
    return {};
}

std::string_view
remove_redundant_prefixes(std::string_view expr) {
    if (expr.starts_with("./"))
    {
        expr.remove_prefix(2);
    }
    else if (expr.starts_with("this."))
    {
        expr.remove_prefix(5);
    }
    else if (expr == "this")
    {
        expr.remove_prefix(4);
    }
    else if (expr == ".")
    {
        expr.remove_prefix(1);
    }
    return expr;
}

Expected<void, Error>
Handlebars::
setupArgs(
    std::string_view expression,
    dom::Value const& context,
    detail::RenderState & state,
    dom::Array &args,
    dom::Object& cb,
    HandlebarsOptions const& opt) const
{
    std::string_view expr;
    // ==========================================
    // Initial setup
    // ==========================================
    cb.set("hash", dom::newObject<dom::DefaultObjectImpl>());
    if (opt.trackIds)
    {
        cb.set("ids", dom::newArray<dom::DefaultArrayImpl>());
        cb.set("hashIds", dom::newObject<dom::DefaultObjectImpl>());
    }
    else
    {
        cb.set("ids", {});
        cb.set("hashIds", {});
    }
    dom::Object hash = cb.get("hash").getObject();
    while (findExpr(expr, expression))
    {
        // ==========================================
        // Find next expression
        // ==========================================
        auto exprEndPos = expr.data() + expr.size() - expression.data();
        expression = expression.substr(exprEndPos);
        if (!expression.empty() && expression.front() != ' ')
        {
          std::string msg = std::format(
              "Parse error. Invalid helper expression. {}{}", expr, expression);
          auto res = find_position_in_text(expression, state.rootTemplateText);
          if (res) {
            return Unexpected(
                Error(msg, res.line, res.column, res.pos));
          }
            return Unexpected(Error(msg));
        }
        expression = trim_ldelimiters(expression, " ");
        auto [k, v] = findKeyValuePair(expr);
        bool const isPositional = k.empty();
        if (isPositional)
        {
            // ==========================================
            // Positional argument
            // ==========================================
            auto _hbs_try11 = evalExpr(context, expr, state, opt, true); if (!_hbs_try11) return Unexpected(_hbs_try11.error()); auto res = *std::move(_hbs_try11);
            args.emplace_back(res.value);
            if (opt.trackIds) {
                dom::Array ids = cb.get("ids").getArray();
                if (res.isLiteral)
                {
                    ids.emplace_back(nullptr);
                }
                else if (res.isSubexpr)
                {
                    ids.emplace_back(true);
                }
                else if (res.fromBlockParams)
                {
                    dom::Value IdVal = expr;
                    state.blockValuePaths.visit(
                        [&](dom::String const& key, dom::Value const& value)
                    {
                        if (expr.starts_with(key))
                        {
                            if (value.isString())
                            {
                                std::string idStr;
                                idStr += value.getString();
                                idStr += expr.substr(key.size());
                                IdVal = idStr;
                            }
                            return false;
                        }
                        return true;
                    });
                    ids.emplace_back(IdVal);
                }
                else
                {
                    ids.emplace_back(remove_redundant_prefixes(expr));
                }
            }
        }
        else
        {
            // ==========================================
            // Named argument
            // ==========================================
            auto _hbs_try12 = evalExpr(context, v, state, opt, true); if (!_hbs_try12) return Unexpected(_hbs_try12.error()); auto res = *std::move(_hbs_try12);
            hash.set(k, res.value);
            if (opt.trackIds) {
                dom::Object hashIds = cb.get("hashIds").getObject();
                if (res.isLiteral) {
                    hashIds.set(k, nullptr);
                }
                else if (res.isSubexpr) {
                    hashIds.set(k, true);
                }
                else {
                    hashIds.set(k, remove_redundant_prefixes(v));
                }
            }
        }
    }
    cb.set("lookupProperty", dom::makeInvocable([&state, &opt](
        dom::Value const& obj, dom::Value const& field) -> dom::Value
    {
        return lookupPropertyImpl(obj, field, state, opt).value().first;
    }));
    args.emplace_back(cb);
    return {};
}

Expected<void, Error>
Handlebars::
renderDecorator(
    Handlebars::Tag const& tag,
    OutputRef &out,
    dom::Value const& context,
    HandlebarsOptions const& opt,
    detail::RenderState& state) const {
    // ==============================================================
    // Validate decorator
    // ==============================================================
    if (tag.helper != "inline")
    {
      out << std::format(R"([undefined decorator "{}" in "{}"])", tag.helper,
                         tag.buffer);
      return {};
    }

    // ==============================================================
    // Evaluate expression
    // ==============================================================
    std::string_view expr;
    findExpr(expr, tag.arguments);
    auto _hbs_try13 = evalExpr(context, expr, state, opt, true); if (!_hbs_try13) return Unexpected(_hbs_try13.error()); auto res = *std::move(_hbs_try13);
    if (!res.value.isString())
    {
      out << std::format(R"([invalid decorator expression "{}" in "{}"])",
                         tag.arguments, tag.buffer);
      return {};
    }
    std::string_view partial_name = res.value.getString();

    // ==============================================================
    // Parse block
    // ==============================================================
    std::string_view fnBlock;
    std::string_view inverseBlock;
    Tag inverseTag;
    if (tag.type2 == '#') {
        if (!parseBlock(tag.helper, tag, opt, state, state.templateText, out, fnBlock, inverseBlock, inverseTag, false))
        {
            return {};
        }
    }
    fnBlock = trim_rspaces(fnBlock);
    state.inlinePartials.back()[std::string(partial_name)] = fnBlock;
    return {};
}

Expected<void, Error>
Handlebars::
renderPartial(
    Handlebars::Tag const &tag,
    OutputRef &out,
    dom::Value const &context,
    HandlebarsOptions const& opt,
    detail::RenderState& state) const
{
    // ==============================================================
    // Evaluate partial name
    // ==============================================================
    std::string partialName(tag.helper);
    bool const isDynamicPartial = partialName.starts_with('(');
    bool const isEscapedPartialName =
        !partialName.empty() &&
        partialName.front() == '[' &&
        partialName.back() == ']';
    if (isDynamicPartial)
    {
        std::string_view expr;
        findExpr(expr, partialName);
        auto _hbs_try14 = evalExpr(context, expr, state, opt, true); if (!_hbs_try14) return Unexpected(_hbs_try14.error()); auto res = *std::move(_hbs_try14);
        if (res.value.isString())
        {
            partialName = res.value.getString();
        }
    }
    else if (isEscapedPartialName)
    {
        partialName = partialName.substr(1, partialName.size() - 2);
    }
    else if (is_literal_string(partialName))
    {
        partialName = unescapeString(partialName);
    }

    // ==============================================================
    // Parse Block
    // ==============================================================
    std::string_view fnBlock;
    std::string_view inverseBlock;
    Tag inverseTag;
    if (tag.type2 == '#')
    {
        if (!parseBlock(tag.helper, tag, opt, state, state.templateText, out, fnBlock, inverseBlock, inverseTag, false))
        {
            return {};
        }
    }

    // ==============================================================
    // Find registered partial content
    // ==============================================================
    auto [partial_content, found] = getPartial(partialName, state);
    if (!found)
    {
        if (tag.type2 == '#')
        {
            partial_content = fnBlock;
        }
        else
        {
          return Unexpected(Error(
              std::format("The partial {} could not be found", partialName)));
        }
    }

    // ==============================================================
    // Evaluate partial block
    // ==============================================================
    if (tag.type2 == '#')
    {
        // ==========================================================
        // Extract inline partials
        // ==========================================================
        state.inlinePartials.emplace_back();
        OutputRef dumb{};
        std::string_view templateText = state.templateText;
        state.templateText = fnBlock;
        { auto _hbs_try15 = this->try_render_to_impl(dumb, context, opt, state); if (!_hbs_try15) return Unexpected(_hbs_try15.error()); }
        state.templateText = templateText;

        // ==========================================================
        // Set @partial-block
        // ==========================================================
        using diff_type = std::vector<std::string_view>::difference_type;
        state.partialBlocks.insert(
            state.partialBlocks.begin() +
                static_cast<diff_type>(state.partialBlockLevel),
            fnBlock);
        ++state.partialBlockLevel;
    }

    // ==============================================================
    // Setup partial context
    // ==============================================================
    // Default context
    dom::Value partialCtx = dom::Object{};
    if (!opt.explicitPartialContext)
    {
        if (context.isObject())
        {
            partialCtx = createFrame(context.getObject());
        }
        else
        {
            partialCtx = context;
        }
    }

    // ==========================================
    // Populate with arguments
    // ==========================================
    bool partialCtxChanged = false;
    dom::Value prevContextPath = state.context.get("contextPath");
    if (!tag.arguments.empty())
    {
        // create context from specified keys
        auto tagContent = tag.arguments;
        std::string_view expr;
        while (findExpr(expr, tagContent))
        {
            tagContent = tagContent.substr(expr.data() + expr.size() - tagContent.data());
            auto [partialKey, contextKey] = findKeyValuePair(expr);
            bool const isContextReplacement = partialKey.empty();
            if (isContextReplacement)
            {
                // ==========================================
                // Replace context
                // ==========================================
                // Check if context has been replaced before
                if (partialCtxChanged)
                {
                    std::size_t n = 2;
                    while (findExpr(expr, tagContent))
                    {
                        auto [partialKey2, _] = findKeyValuePair(expr);
                        if (!partialKey2.empty()) {
                            break;
                        }
                        ++n;
                    }
                    std::string msg = std::format(
                        "Unsupported number of partial arguments: {}", n);
                    auto res = find_position_in_text(state.rootTemplateText, tag.buffer);
                    if (res)
                    {
                        return Unexpected(Error(msg, res.line, res.column, res.pos));
                    }
                    return Unexpected(Error(msg));
                }

                // Do change the context
                auto _hbs_try16 = evalExpr(context, expr, state, opt, true); if (!_hbs_try16) return Unexpected(_hbs_try16.error()); auto res = *std::move(_hbs_try16);
                if (opt.trackIds)
                {
                    std::string contextPath = detail::appendContextPath(
                        state.context.get("contextPath"), expr);
                    state.context.set("contextPath", contextPath);
                }
                if (res.found)
                {
                    if (res.value.isObject())
                    {
                        partialCtx = createFrame(res.value.getObject());
                    }
                    else
                    {
                        partialCtx = res.value;
                    }
                }
                partialCtxChanged = true;
                continue;
            }

            // ==========================================
            // Add named argument to context
            // ==========================================
            evalExprResult res;
            if (contextKey != ".")
            {
                auto _hbs_try17 = evalExpr(context, contextKey, state, opt, true); if (!_hbs_try17) return Unexpected(_hbs_try17.error()); res = *std::move(_hbs_try17);
            }
            else
            {
                res.value = context;
                res.found = true;
                res.isLiteral = false;
            }
            if (res.found)
            {
                bool const needs_reset_context = !partialCtx.isObject();
                if (needs_reset_context)
                {
                    if (!opt.explicitPartialContext &&
                        context.isObject())
                    {
                        partialCtx = createFrame(context.getObject());
                    }
                    else
                    {
                        partialCtx = dom::Object{};
                    }
                }
                partialCtx.getObject().set(partialKey, res.value);
            }

            if (opt.trackIds)
            {
                // should invalidate context for partials with parameters
                state.context.set("contextPath", true);
            }
        }
    }

    // ==============================================================
    // Render partial
    // ==============================================================
    // ==========================================
    // Setup partial state
    // ==========================================
    std::string_view rootTemplateText = state.rootTemplateText;
    state.rootTemplateText = partial_content;
    std::string_view templateText = state.templateText;
    state.templateText = partial_content;
    bool const isPartialBlock = partialName == "@partial-block";
    state.partialBlockLevel -= isPartialBlock;
    out.setIndent(out.getIndent() + tag.standaloneIndent * !opt.preventIndent);
    if (partialCtxChanged)
    {
        state.parentContext.emplace_back(context);
    }
    state.contextStack.emplace_back(state.context);

    // ==========================================
    // Render partial
    // ==========================================
    { auto _hbs_try18 = this->try_render_to_impl(out, partialCtx, opt, state); if (!_hbs_try18) return Unexpected(_hbs_try18.error()); }

    // ==========================================
    // Restore state
    // ==========================================
    if (partialCtxChanged)
    {
        state.parentContext.pop_back();
    }
    state.contextStack.pop_back();
    out.setIndent(out.getIndent() - tag.standaloneIndent * !opt.preventIndent);
    state.partialBlockLevel += isPartialBlock;
    state.templateText = templateText;
    state.rootTemplateText = rootTemplateText;
    if (opt.trackIds && partialCtxChanged)
    {
        state.context.set("contextPath", prevContextPath);
    }

    if (tag.type2 == '#')
    {
        state.inlinePartials.pop_back();
        --state.partialBlockLevel;
        using diff_type = std::vector<std::string_view>::difference_type;
        auto it = state.partialBlocks.begin() +
              static_cast<diff_type>(state.partialBlockLevel);
        state.partialBlocks.erase(it);
    }

    // ==============================================================
    // Remove partial standalone whitespace
    // ==============================================================
    if (!opt.ignoreStandalone && tag.isStandalone)
    {
        state.templateText = trim_ldelimiters(state.templateText, " ");
        if (state.templateText.starts_with('\n'))
        {
            state.templateText.remove_prefix(1);
        }
        else if (state.templateText.starts_with("\r\n"))
        {
            state.templateText.remove_prefix(2);
        }
    }
    return {};
}

Expected<void, Error>
Handlebars::
renderBlock(
    std::string_view blockName,
    Handlebars::Tag const &tag,
    OutputRef &out,
    dom::Value const& context,
    HandlebarsOptions const& opt,
    detail::RenderState& state,
    bool isChainedBlock) const {
    if (tag.removeRWhitespace) {
        state.templateText = trim_lspaces(state.templateText);
    }

    // ==============================================================
    // Parse block
    // ==============================================================
    std::string_view fnBlock;
    std::string_view inverseBlock;
    Tag inverseTag;
    { auto _hbs_try19 = parseBlock(
        blockName, tag, opt, state, state.templateText, out,
        fnBlock, inverseBlock, inverseTag, isChainedBlock); if (!_hbs_try19) return Unexpected(_hbs_try19.error()); }

    // ==============================================================
    // Find helper
    // ==============================================================
    bool const isNoArgBlock = tag.arguments.empty();
    auto [fn, found] = getHelper(tag.helper, isNoArgBlock);
    bool const useContextFunction = !found && !tag.arguments.empty();
    if (useContextFunction)
    {
        auto _hbs_try20 = evalExpr(context, tag.helper, state, opt, false); if (!_hbs_try20) return Unexpected(_hbs_try20.error()); auto res = *std::move(_hbs_try20);
        if (res.found && res.value.isFunction())
        {
            fn = res.value.getFunction();
            found = true;
        }
    }

    std::string_view tagArgumentsStr = tag.arguments;
    bool const emulateMustache = !found && isNoArgBlock;
    std::string unescaped;
    if (emulateMustache)
    {
        // =========================================================
        // Emulate mustache: helper expression becomes the argument
        // =========================================================
        if (is_literal_string(tag.helper))
        {
            unescaped = unescapeString(tag.helper);
            tagArgumentsStr = unescaped;
        }
        else
        {
            tagArgumentsStr = tag.helper;
        }
    }
    else if (opt.strict && !found)
    {
        // ============================================
        // Strict mode: throw when helper is not found
        // ============================================
        std::string msg = std::format("\"{}\" not defined in {}", tag.helper,
                                      toString(context));
        auto res = find_position_in_text(state.rootTemplateText, tag.helper);
        if (res)
        {
            return Unexpected(Error(msg, res.line, res.column, res.pos));
        }
        return Unexpected(Error(msg));
    }

    // ==============================================================
    // Setup helper context
    // ==============================================================
    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
    cb.set("name", tag.helper);
    cb.set("context", context);
    dom::Object data;
    if (!state.context.empty())
    {
        data = createFrame(state.context);
    }
    data.set("root", state.rootContext);
    cb.set("data", data);
    cb.set("log", logger_);
    HandlebarsOptions noStrict = opt;
    noStrict.strict = opt.strict && emulateMustache;
    setupArgs(tagArgumentsStr, context, state, args, cb, noStrict);

    // ==========================================
    // Setup block parameters
    // ==========================================
    std::string_view expr;
    std::string_view bps = tag.blockParams;

    std::vector<std::string_view> blockParamIds;
    while (findExpr(expr, bps))
    {
        bps = bps.substr(expr.data() + expr.size() - bps.data());
        blockParamIds.emplace_back(expr);
    }
    cb.set("blockParams", blockParamIds.size());

    // ==============================================================
    // Setup callbacks
    // ==============================================================
    auto write_nested_block =
        [this, fnBlock, opt, &state, &context, &blockParamIds](
            OutputRef out,
            dom::Value newContext,
            dom::Value const& options) -> Expected<void, Error>
    {
        // ==========================================
        // Setup new render state
        // ==========================================
        std::string_view templateText = state.templateText;
        state.templateText = fnBlock;
        dom::Object prevStateData = state.context;
        dom::Object prevBlockValues = state.blockValues;
        dom::Object prevBlockValuePaths = state.blockValuePaths;

        // Context
        if (newContext.isUndefined())
        {
            newContext = context;
        }
        bool const sameContext =
            &newContext == &context ||
            (newContext.isObject() && context.isObject() && newContext.getObject().impl() == context.getObject().impl()) ||
            (newContext.isArray() && context.isArray() && newContext.getArray().impl() == context.getArray().impl());
        if (!sameContext)
        {
            state.parentContext.push_back(context);
        }

        // Extra options from the helper
        if (options.isObject())
        {
            dom::Object const& optObj = options.getObject();

            // Data
            if (optObj.exists("data"))
            {
                dom::Value dataV = optObj.get("data");
                if (dataV.isObject())
                {
                    state.context = dataV.getObject();
                }
            }

            // Block params
            if (optObj.exists("blockParams"))
            {
                dom::Value blockParamsV = optObj.get("blockParams");
                if (blockParamsV.isArray())
                {
                    dom::Object newBlockValues;
                    dom::Array const& blockParams = blockParamsV.getArray();
                    for (std::size_t i = 0; i < blockParamIds.size(); ++i) {
                        newBlockValues.set(blockParamIds[i], blockParams.get(i));
                    }
                    dom::Object blockValuesOverlay =
                        createFrame(newBlockValues, state.blockValues);
                    state.blockValues = std::move(blockValuesOverlay);
                }
            }

            // Block param paths
            if (optObj.exists("blockParamPaths"))
            {
                dom::Value blockParamPathsV = optObj.get("blockParamPaths");
                if (blockParamPathsV.isArray())
                {
                    dom::Array const& blockParamPaths = blockParamPathsV.getArray();
                    dom::Object newBlockValuePaths;
                    for (std::size_t i = 0; i < blockParamIds.size(); ++i)
                    {
                        newBlockValuePaths.set(blockParamIds[i], blockParamPaths.get(i));
                    }
                    dom::Object blockValuePathsOverlay =
                        createFrame(newBlockValuePaths, state.blockValuePaths);
                    state.blockValuePaths = std::move(blockValuePathsOverlay);
                }
            }
        }

        // ==========================================
        // Render
        // ==========================================
        { auto _hbs_try21 = try_render_to_impl(out, newContext, opt, state); if (!_hbs_try21) return Unexpected(_hbs_try21.error()); }

        // ==========================================
        // Restore state
        // ==========================================
        state.templateText = templateText;
        state.context = std::move(prevStateData);
        state.blockValues = std::move(prevBlockValues);
        state.blockValuePaths = std::move(prevBlockValuePaths);
        if (!sameContext)
        {
            state.parentContext.pop_back();
        }
        return {};
    };

    auto write_inverse_block =
        [this, inverseTag, inverseBlock, opt, blockName, &state, &context, &blockParamIds](
            OutputRef out,
            dom::Value const& newContext,
            dom::Value const& options) -> Expected<void, Error>
    {
        // ==========================================
        // Setup new render state
        // ==========================================
        std::string_view templateText = state.templateText;
        state.templateText = inverseBlock;
        dom::Object prevStateData = state.context;
        dom::Object prevBlockValues = state.blockValues;
        dom::Object prevBlockValuePaths = state.blockValuePaths;

        // Context
        bool const sameContext =
            &newContext == &context ||
            (newContext.isObject() && context.isObject() && newContext.getObject().impl() == context.getObject().impl()) ||
            (newContext.isArray() && context.isArray() && newContext.getArray().impl() == context.getArray().impl());
        if (!sameContext)
        {
            state.parentContext.push_back(context);
        }

        // Extra options from the helper
        if (options.isObject())
        {
            dom::Object const& optObj = options.getObject();

            // Data
            if (optObj.exists("data"))
            {
                dom::Value dataV = optObj.get("data");
                if (dataV.isObject())
                {
                    state.context = dataV.getObject();
                }
            }

            // Block params
            if (optObj.exists("blockParams"))
            {
                dom::Value blockParamsV = optObj.get("blockParams");
                if (blockParamsV.isArray())
                {
                    dom::Object newBlockValues;
                    dom::Array const& blockParams = blockParamsV.getArray();
                    for (std::size_t i = 0; i < blockParamIds.size(); ++i) {
                        newBlockValues.set(blockParamIds[i], blockParams.get(i));
                    }
                    dom::Object blockValuesOverlay =
                        createFrame(newBlockValues, state.blockValues);
                    state.blockValues = std::move(blockValuesOverlay);
                }
            }

            // Block param paths
            if (optObj.exists("blockParamPaths"))
            {
                dom::Value blockParamPathsV = optObj.get("blockParamPaths");
                if (blockParamPathsV.isArray())
                {
                    dom::Array const& blockParamPaths = blockParamPathsV.getArray();
                    dom::Object newBlockValuePaths;
                    for (std::size_t i = 0; i < blockParamIds.size(); ++i)
                    {
                        newBlockValuePaths.set(blockParamIds[i], blockParamPaths.get(i));
                    }
                    dom::Object blockValuePathsOverlay =
                        createFrame(newBlockValuePaths, state.blockValuePaths);
                    state.blockValuePaths = std::move(blockValuePathsOverlay);
                }
            }
        }

        // ==========================================
        // Render
        // ==========================================
        bool const plainInverse = inverseTag.helper.empty();
        if (plainInverse) {
            // Inverse tag does not contain its own helper
            // i.e. {{#helper}}...{{^}}...{{/helper}} instead of
            //      {{#helper}}...{{^helper2}}...{{/helper}}
            // Render the inverse block with the specified context
            if (!opt.ignoreStandalone && inverseTag.isStandalone)
            {
                state.templateText = trim_ldelimiters(state.templateText, " ");
                if (state.templateText.starts_with('\n'))
                {
                    state.templateText.remove_prefix(1);
                }
                else if (state.templateText.starts_with("\r\n"))
                {
                    state.templateText.remove_prefix(2);
                }
            }
            { auto _hbs_try22 = try_render_to_impl(out, newContext, opt, state); if (!_hbs_try22) return Unexpected(_hbs_try22.error()); }
        }
        else
        {
            // Inverse tag contains its own helper
            // Go straight to the render block method, which will
            // expect a closing tag matching the parent tag, and
            // interpret sequential "else" tags as more chained
            // inverse tags.
            { auto _hbs_try23 = renderBlock(blockName, inverseTag, out, newContext, opt, state, true); if (!_hbs_try23) return Unexpected(_hbs_try23.error()); }
        }

        // ==========================================
        // Restore state
        // ==========================================
        state.templateText = templateText;
        state.context = std::move(prevStateData);
        state.blockValues = std::move(prevBlockValues);
        state.blockValuePaths = std::move(prevBlockValuePaths);
        if (!sameContext)
        {
            state.parentContext.pop_back();
        }
        return {};
    };

    std::optional<Error> hbs_error;
    if (!tag.rawBlock) {
        cb.set("write", dom::makeInvocable([&out, &write_nested_block, &hbs_error](
            dom::Value const& newContext,
            dom::Value const& options) -> dom::Expected<dom::Value> {
            auto exp = write_nested_block(out, newContext, options);
            if (!exp)
            {
                hbs_error = exp.error();
                return Unexpected(dom::Error("Error in block helper"));
            }
            return dom::Value();
        }));
        cb.set("fn", dom::makeInvocable([&write_nested_block, &hbs_error](
            dom::Value const& newContext,
            dom::Value const& options) -> dom::Expected<dom::Value> {
            std::string out;
            OutputRef out2(out);
            auto exp = write_nested_block(out2, newContext, options);
            if (!exp)
            {
                hbs_error = exp.error();
                return Unexpected(dom::Error("Error in block helper"));
            }
            return out;
        }));
        cb.set("write_inverse", dom::makeInvocable([&out, &write_inverse_block, &hbs_error](
            dom::Value const& newContext,
            dom::Value const& options) -> dom::Expected<dom::Value> {
            auto exp = write_inverse_block(out, newContext, options);
            if (!exp)
            {
                hbs_error = exp.error();
                return Unexpected(dom::Error("Error in block helper"));
            }
            return dom::Value();
        }));
        cb.set("inverse", dom::makeInvocable([&write_inverse_block, &hbs_error](
            dom::Value const& newContext,
            dom::Value const& options) -> dom::Expected<dom::Value> {
            std::string out;
            OutputRef out2(out);
            auto exp = write_inverse_block(out2, newContext, options);
            if (!exp)
            {
                hbs_error = exp.error();
                return Unexpected(dom::Error("Error in block helper"));
            }
            return out;
        }));
    }
    else
    {
        cb.set("fn", dom::makeInvocable([fnBlock]() {
            return fnBlock;
        }));
        cb.set("write", dom::makeInvocable([&out, fnBlock]() {
            out << fnBlock;
            return dom::Value();
        }));
        // noop: No inverseBlock for raw block
        cb.set("write_inverse", dom::makeInvocable([]() {
            return dom::Value();
        }));
        cb.set("inverse", dom::makeInvocable([]() -> dom::String {
            return "";
        }));
    }

    bool const isStandaloneInvertedSection = tag.type == '^' && !isChainedBlock;
    if (isStandaloneInvertedSection)
    {
        auto fnV = cb.get("fn");
        auto inverse = cb.get("inverse");
        cb.set("fn", inverse);
        cb.set("inverse", fnV);
        auto fn_write = cb.get("write");
        auto write_inverse = cb.get("write_inverse");
        cb.set("write", write_inverse);
        cb.set("write_inverse", fn_write);
    }

    // ==============================================================
    // Call helper
    // ==============================================================
    if (emulateMustache && args.get(0).isFunction())
    {
        // When emulating mustache, if the first argument
        // is a function, we call this function before
        // passing it to blockHelperMissing
        args.set(0, args.get(0)(cb));
    }
    state.inlinePartials.emplace_back();
    // state.parentContext.emplace_back(context);
    state.contextStack.emplace_back(state.context);
    Expected<dom::Value> exp2 = fn.call(args);
    if (!exp2)
    {
        if (hbs_error)
        {
            // The error happened when the helper called fn or inverse,
            // and we have the exact Error for that.
            return Unexpected(*hbs_error);
        }

        // The helper returned a regular Error, and we convert it to
        // a Error with the same message and the position of
        // the helper call.
        Error e = exp2.error();
        auto res = find_position_in_text(state.rootTemplateText, tag.buffer);
        if (res)
        {
            return Unexpected(Error(e.message(), res.line, res.column, res.pos));
        }
        return Unexpected(Error(e.message()));
    }
    dom::Value res = *exp2;
    if (!res.isUndefined()) {
        // Block helpers are always unescaped
        HandlebarsOptions opt2 = opt;
        opt2.noEscape = true;
        format_to(out, res, opt2);
    }
    state.inlinePartials.pop_back();
    // state.parentContext.pop_back();
    state.contextStack.pop_back();
    return {};
}

void
Handlebars::
registerPartial(
    std::string_view name,
    std::string_view text)
{
    auto it = partials_.find(name);
    if (it != partials_.end())
        partials_.erase(it);
    partials_.emplace(std::string(name), std::string(text));
}

void
Handlebars::
registerHelper(std::string_view name, dom::Function const& helper)
{
    auto it = helpers_.find(name);
    if (it != helpers_.end())
        helpers_.erase(it);
    helpers_.emplace(std::string(name), helper);
}


void
Handlebars::
registerLogger(dom::Function fn)
{
    logger_ = std::move(fn);
}

void
Handlebars::
unregisterHelper(std::string_view name) {
    if (auto it = helpers_.find(name); it != helpers_.end())
    {
        helpers_.erase(it);
    }

    // Re-register mandatory helpers
    if (name == "helperMissing")
    {
        registerHelper(
            "helperMissing",
            dom::makeVariadicInvocable(
                helpers::helper_missing_fn));
    }
    else if (name == "blockHelperMissing")
    {
        registerHelper(
            "blockHelperMissing",
            dom::makeInvocable(
                helpers::block_helper_missing_fn));
    }
}

} // namespace handlebars
} // namespace mrdocs


