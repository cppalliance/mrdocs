//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/Handlebars.hpp>
#include <mrdox/Support/Path.hpp>
#include <fmt/format.h>
#include <ranges>
#include <charconv>
#include <array>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <unordered_set>
#include <utility>

namespace clang {
namespace mrdox {

// ==============================================================
// Output
// ==============================================================
OutputRef&
OutputRef::
write_impl( std::string_view sv )
{
    if (indent_ == 0)
    {
        fptr_( out_, sv );
        return *this;
    }

    std::size_t pos = sv.find('\n');
    if (pos == std::string_view::npos)
    {
        fptr_( out_, sv );
        return *this;
    }

    fptr_( out_, sv.substr(0, pos + 1) );
    ++pos;
    while (pos < sv.size())
    {
        for (std::size_t i = 0; i < indent_; ++i)
        {
            fptr_( out_, std::string_view(" ") );
        }
        std::size_t next = sv.find('\n', pos);
        if (next == std::string_view::npos)
        {
            fptr_( out_, sv.substr(pos) );
            return *this;
        }
        fptr_( out_, sv.substr(pos, next - pos + 1) );
        pos = next + 1;
    }
    return *this;
}

// ==============================================================
// Utility functions
// ==============================================================

bool
isTruthy(dom::Value const& arg)
{
    switch (arg.kind()) {
        case dom::Kind::Boolean:
            return arg.getBool();
        case dom::Kind::Integer:
            return arg.getInteger() != 0;
        case dom::Kind::String:
            return !arg.getString().empty();
        case dom::Kind::Array:
        case dom::Kind::Object:
            return true;
        case dom::Kind::Null:
            return false;
        default:
            MRDOX_UNREACHABLE();
    }
}

bool
isEmpty(dom::Value const& arg)
{
    if (arg.isArray())
        return arg.getArray().empty();
    if (arg.isInteger())
        return false;
    return !isTruthy(arg);
}

class OverlayObjectImpl : public dom::ObjectImpl
{
    dom::Object parent_;
    dom::Object child_;

public:
    ~OverlayObjectImpl() override = default;

    OverlayObjectImpl(dom::Object parent)
        : parent_(std::move(parent))
    {}

    OverlayObjectImpl(dom::Object child, dom::Object parent)
        : parent_(std::move(parent))
        , child_(std::move(child))
    {}

    std::size_t size() const override {
        std::size_t n = parent_.size() + child_.size();
        for (auto const& [key, value] : child_) {
            if (parent_.exists(key))
                    --n;
            }
        return n;
    };

    reference get(std::size_t i) const override {
        if (i < child_.size())
                return child_.get(i);
            return parent_.get(i - child_.size());
    };

    dom::Value find(std::string_view key) const override {
        if (child_.exists(key))
            return child_.find(key);
        if (parent_.exists(key))
            return parent_.find(key);
        return nullptr;
    }

    void set(dom::String key, dom::Value value) override {
        child_.set(key, std::move(value));
    };
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

detail::safeStringWrapper
safeString(std::string_view str)
{
    detail::safeStringWrapper w;
    w.v_ = str;
    return w;
}

void
escapeExpression(
    OutputRef out,
    std::string_view str,
    HandlebarsOptions opt)
{
    if (opt.noEscape)
    {
        out << str;
    }
    else
    {
        escapeExpression(out, str);
    }
}

void
escapeExpression(
    OutputRef out,
    std::string_view str)
{
    for (auto c : str)
    {
        switch (c)
        {
        case '&':
            out << "&amp;";
            break;
        case '<':
            out << "&lt;";
            break;
        case '>':
            out << "&gt;";
            break;
        case '"':
            out << "&quot;";
            break;
        case '\'':
            out << "&#x27;";
            break;
        case '`':
            out << "&#x60;";
            break;
        default:
            out << c;
            break;
        }
    }
}

std::string
escapeExpression(
    std::string_view str)
{
    std::string res;
    OutputRef out(res);
    escapeExpression(out, str);
    return res;
}

void
format_to(
    OutputRef out,
    dom::Value const& value,
    HandlebarsOptions opt)
{
    if (value.isString()) {
        escapeExpression(out, value.getString(), opt);
    } else if (value.isInteger()) {
        out << value.getInteger();
    } else if (value.isBoolean()) {
        if (value.getBool()) {
            out << "true";
        } else {
            out << "false";
        }
    } else if (value.isArray()) {
        out << "[";
        dom::Array const& array = value.getArray();
        if (!array.empty()) {
            format_to(out, array.at(0), opt);
            for (std::size_t i = 1; i < array.size(); ++i) {
                out << ",";
                format_to(out, array.at(i), opt);
            }
        }
        out << "]";
    } else if (value.isObject()) {
        out << "[object Object]";
    }
}

constexpr
std::string_view
trim_delimiters(std::string_view expression, std::string_view delimiters)
{
    auto pos = expression.find_first_not_of(delimiters);
    if (pos == std::string_view::npos)
        return "";
    expression.remove_prefix(pos);
    pos = expression.find_last_not_of(delimiters);
    if (pos == std::string_view::npos)
        return "";
    expression.remove_suffix(expression.size() - pos - 1);
    return expression;
}

constexpr
std::string_view
trim_ldelimiters(std::string_view expression, std::string_view delimiters)
{
    auto pos = expression.find_first_not_of(delimiters);
    if (pos == std::string_view::npos)
        return "";
    expression.remove_prefix(pos);
    return expression;
}

constexpr
std::string_view
trim_rdelimiters(std::string_view expression, std::string_view delimiters)
{
    auto pos = expression.find_last_not_of(delimiters);
    if (pos == std::string_view::npos)
        return "";
    expression.remove_suffix(expression.size() - pos - 1);
    return expression;
}

constexpr
std::string_view
trim_spaces(std::string_view expression)
{
    return trim_delimiters(expression, " \t\r\n");
}

constexpr
std::string_view
trim_lspaces(std::string_view expression)
{
    return trim_ldelimiters(expression, " \t\r\n");
}

constexpr
std::string_view
trim_rspaces(std::string_view expression)
{
    return trim_rdelimiters(expression, " \t\r\n");
}

// ==============================================================
// Helper Callback
// ==============================================================

namespace detail {
    struct RenderState
    {
        std::string_view templateText0;
        std::string_view templateText;
        std::vector<detail::partials_view_map> inlinePartials;
        std::vector<std::string_view> partialBlocks;
        std::size_t partialBlockLevel = 0;
        dom::Object data;
        dom::Object blockValues;
        std::vector<dom::Value> compatStack;
    };
}

std::string
HandlebarsCallback::
fn(dom::Value const& context,
   dom::Object const& data,
   dom::Object const& blockValues) const {
    if (isBlock()) {
        std::string result;
        OutputRef out(result);
        fn_(out, context, data, blockValues);
        return result;
    } else {
        return {};
    }
}

void
HandlebarsCallback::
fn(OutputRef out,
   dom::Value const& context,
   dom::Object const& data,
   dom::Object const& blockValues) const {
    if (isBlock()) {
        fn_(out, context, data, blockValues);
    }
}

std::string
HandlebarsCallback::
fn(dom::Value const& context) const {
    return fn(context, data(), dom::Object{});
}

void
HandlebarsCallback::
fn(OutputRef out, dom::Value const& context) const {
    fn(out, context, data(), dom::Object{});
}

std::string
HandlebarsCallback::
inverse(dom::Value const& context,
   dom::Object const& data,
   dom::Object const& blockValues) const {
    if (isBlock()) {
        std::string result;
        OutputRef out(result);
        inverse_(out, context, data, blockValues);
        return result;
    } else {
        return {};
    }
}

void
HandlebarsCallback::
inverse(OutputRef out,
   dom::Value const& context,
   dom::Object const& data,
   dom::Object const& blockValues) const {
    if (isBlock()) {
        inverse_(out, context, data, blockValues);
    }
}

std::string
HandlebarsCallback::
inverse(dom::Value const& context) const {
    return inverse(context, data(), dom::Object{});
}

void
HandlebarsCallback::
inverse(OutputRef out, dom::Value const& context) const {
    inverse(out, context, data(), dom::Object{});
}

void
HandlebarsCallback::
log(dom::Value const& level,
    dom::Array const& args) const {
    MRDOX_ASSERT(logger_);
    (*logger_)(level, args);
}

bool
isCurrentContextSegment(std::string_view path)
{
    return path == "." || path == "this";
}

std::string_view
popFirstSegment(std::string_view& path0)
{
    // Skip dot segments
    std::string_view path = path0;
    while (path.starts_with("./") || path.starts_with("[.]/") || path.starts_with("[.]."))
    {
        path.remove_prefix(path.front() == '.' ? 2 : 4);
    }

    // All that's left is a dot segment, so there are no more valid paths
    if (path == "." || path == "[.]")
    {
        path0 = {};
        return {};
    }

    // If path starts with "[" the logic changes for literal segments
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

    // Check if we have a literal number segment, in which case the dots
    // are part of the segment
    if (std::ranges::all_of(path, [](char c) { return c == '.' || std::isdigit(c); }))
    {
        if (std::ranges::count(path, '.') < 2)
        {
            // Number segment
            path0 = {};
            return path;
        }
    }

    // If path starts with dotdot segment, the delimiter needs to be a slash
    // Otherwise, it can be either a slash or a dot
    std::string_view delimiters = path.starts_with("..") ? "/" : "./";
    auto pos = path.find_first_of(delimiters);
    // If no delimiter, the whole path is the segment
    if (pos == std::string_view::npos)
    {
        path0 = {};
        return path;
    }
    // Otherwise, the segment is up to the delimiter
    path0 = path.substr(pos + 1);
    return path.substr(0, pos);
}

struct position_in_text
{
    std::size_t line = std::size_t(-1);
    std::size_t column = std::size_t(-1);
    std::size_t pos = std::size_t(-1);

    constexpr
    operator bool() const
    {
        return line != std::size_t(-1);
    }
};

constexpr
position_in_text
find_position_in_text(
    std::string_view text,
    std::string_view substr)
{
    position_in_text res;
    if (substr.data() >= text.data() &&
        substr.data() <= text.data() + text.size())
    {
        res.pos = substr.data() - text.data();
        res.line = std::ranges::count(text.substr(0, res.pos), '\n') + 1;
        if (res.line == 1)
            res.column = res.pos;
        else
            res.column = res.pos - text.rfind('\n', res.pos);
    }
    return res;
}

void
checkPath(std::string_view path0, detail::RenderState const& state)
{
    std::string_view path = path0;
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
            auto res = find_position_in_text(state.templateText0, path0);
            if (res)
            {
                throw HandlebarsError(msg, res.line, res.column, res.pos);
            }
            throw HandlebarsError(msg);
        }
        seg = popFirstSegment(path);
    }
}

std::pair<dom::Value, bool>
lookupPropertyImpl(
    dom::Object const& context,
    std::string_view path,
    detail::RenderState const& state)
{
    // Get first value from Object
    std::string_view segment = popFirstSegment(path);
    bool isLiteral = segment.starts_with('[') && segment.ends_with(']');
    std::string_view literalSegment = segment.substr(1 * isLiteral, segment.size() - 2 * isLiteral);
    dom::Value cur = nullptr;
    if (isCurrentContextSegment(segment))
    {
        cur = context;
    }
    else if (!context.exists(literalSegment))
    {
        return {nullptr, false};
    }
    else
    {
        cur = context.find(literalSegment);
    }

    // Recursively get more values from current value
    segment = popFirstSegment(path);
    isLiteral = segment.starts_with('[') && segment.ends_with(']');
    literalSegment = segment.substr(1 * isLiteral, segment.size() - 2 * isLiteral);
    while (!literalSegment.empty())
    {
        // If current value is an Object, get the next value from it
        if (cur.isObject())
        {
            auto obj = cur.getObject();
            if (obj.exists(literalSegment))
                cur = obj.find(literalSegment);
            else
                return {nullptr, false};
        }
        // If current value is an Array, get the next value the stripped index
        else if (cur.isArray())
        {
            size_t index;
            std::from_chars_result res = std::from_chars(
                literalSegment.data(),
                literalSegment.data() + literalSegment.size(),
                index);
            if (res.ec != std::errc())
                return {nullptr, false};
            auto& arr = cur.getArray();
            if (index >= arr.size())
                return {nullptr, false};
            cur = arr.at(index);
        }
        else
        {
            // Current value is not an Object or Array, so we can't get any more
            // segments from it
            return {nullptr, false};
        }
        // Consume more segments to get into the array element
        segment = popFirstSegment(path);
        isLiteral = segment.starts_with('[') && segment.ends_with(']');
        literalSegment = segment.substr(1 * isLiteral, segment.size() - 2 * isLiteral);
    }
    return {cur, true};
}


std::pair<dom::Value, bool>
lookupPropertyImpl(
    dom::Value const& context,
    std::string_view path,
    detail::RenderState const& state) {
    checkPath(path, state);
    if (isCurrentContextSegment(path) || path.empty())
        return {context, true};
    if (context.kind() != dom::Kind::Object) {
        return {nullptr, false};
    }
    return lookupPropertyImpl(context.getObject(), path, state);
}

template <std::convertible_to<std::string_view> S>
std::pair<dom::Value, bool>
lookupPropertyImpl(
    dom::Value const& data,
    S const& path,
    detail::RenderState const& state)
{
    return lookupPropertyImpl(data, std::string_view(path), state);
}

std::pair<dom::Value, bool>
lookupPropertyImpl(
    dom::Value const& context,
    dom::Value const& path,
    detail::RenderState const& state)
{
    if (path.isString())
        return lookupPropertyImpl(context, path.getString(), state);
    if (path.isInteger()) {
        if (context.isArray()) {
            auto& arr = context.getArray();
            if (path.getInteger() >= static_cast<std::int64_t>(arr.size()))
                return {nullptr, false};
            return {arr.at(path.getInteger()), true};
        }
        return lookupPropertyImpl(context, std::to_string(path.getInteger()), state);
    }
    return {nullptr, false};
}

std::pair<dom::Value, bool>
HandlebarsCallback::
lookupProperty(
    dom::Value const& context,
    dom::Value const& path) const
{
    return lookupPropertyImpl(context, path, *renderState_);
}


// ==============================================================
// Engine
// ==============================================================

struct defaultLogger {
    static constexpr std::array<std::string_view, 4> methodMap =
        {"debug", "info", "warn", "error"};
    std::int64_t level_ = 1;

    void
    operator()(dom::Value const& level0, dom::Array const& args) const {
        dom::Value level = lookupLevel(level0);
        if (!level.isInteger()) {
            return;
        }
        if (level.getInteger() > level_) {
            return;
        }
        std::string_view method = methodMap[level.getInteger()];
        std::string out;
        OutputRef os(out);
        os << '[';
        os << method;
        os << ']';
        for (std::size_t i = 0; i < args.size(); ++i) {
            HandlebarsOptions opt;
            opt.noEscape = true;
            format_to(os, args.at(i), opt);
            os << " ";
        }
        fmt::println("{}", out);
    }

    dom::Value
    lookupLevel(dom::Value level) const {
        if (level.isString()) {
            // Find level from map
            std::string levelStr(level.getString());
            // convert level string to lowercase
            auto tolower = [](char c) {
                return c + ('A' - 'a') * (c >= 'a' && c <= 'z');
            };
            std::ranges::transform(levelStr, levelStr.begin(), tolower);
            auto levelMap = std::ranges::find(methodMap, levelStr);
            if (levelMap != methodMap.end()) {
                return std::int64_t(levelMap - methodMap.begin());
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
    registerLogger(defaultLogger{});
}

std::string
Handlebars::
render(
    std::string_view templateText,
    dom::Value const & context,
    HandlebarsOptions const& options) const
{
    std::string out;
    OutputRef os(out);
    render_to(os, templateText, context, options);
    return out;
}

// Find the next handlebars tag
// Returns true if found, false if not found.
// If found, tag is set to the tag text.
bool
findTag(std::string_view &tag, std::string_view templateText)
{
    if (templateText.size() < 4)
        return false;

    // Find opening tag
    auto pos = templateText.find("{{");
    if (pos == std::string_view::npos)
        return false;

    // Find closing tag
    std::string_view closeTagToken = "}}";
    std::string_view closeTagToken2;
    if (templateText.substr(pos).starts_with("{{!--")) {
        closeTagToken = "--}}";
        closeTagToken2 = "--~}}";
    } else if (templateText.substr(pos).starts_with("{{{{")) {
        closeTagToken = "}}}}";
    } else if (templateText.substr(pos).starts_with("{{{")) {
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
        tag = {tag.begin() - 1 - doubleEscaped, tag.end()};
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
    bool forceNoHTMLEscape{false};

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
findExpr(std::string_view & expr, std::string_view tagContent)
{
    tagContent = trim_spaces(tagContent);
    if (tagContent.empty()) {
        expr = tagContent;
        return false;
    }

    // Look for quoted expression
    for (auto quote: {'\"', '\''})
    {
        if (tagContent.front() == quote)
        {
            auto close_pos = tagContent.find(quote, 1);
            while (
                close_pos != std::string_view::npos &&
                tagContent[close_pos - 1] == '\\')
            {
                // Skip escaped quote
                close_pos = tagContent.find(quote, close_pos + 1);
            }
            if (close_pos == std::string_view::npos)
            {
                // No closing quote found, invalid expression
                return false;
            }
            expr = tagContent.substr(0, close_pos + 1);
            return true;
        }
    }

    // Look for subexpressions
    if (tagContent.front() == '(')
    {
        std::string_view all = tagContent.substr(1);
        std::string_view sub;
        while (findExpr(sub, all)) {
            all.remove_prefix(sub.data() + sub.size() - all.data());
        }
        if (!all.starts_with(')'))
            return false;
        expr = tagContent.substr(0, sub.data() + sub.size() - tagContent.data() + 1);
        return true;
    }

    // No quoted expression found, look for whitespace
    auto pos = tagContent.find_first_of(" \t\r\n)");
    if (pos == std::string_view::npos)
    {
        expr = tagContent;
        return true;
    }

    // Check if whitespace is not inside a "[*]" path segment
    auto openBracketPos = tagContent.substr(0, pos).find_first_of('[');
    if (openBracketPos == std::string_view::npos)
    {
        expr = tagContent.substr(0, pos);
        return pos != 0;
    }

    // Consume more "[*]" path segments
    while (pos != std::string_view::npos)
    {
        std::string_view subexpr = tagContent.substr(0, pos);
        auto obrakets = std::count(subexpr.begin(), subexpr.end(), '[');
        auto cbrakets = std::count(subexpr.begin(), subexpr.end(), ']');
        if (obrakets == cbrakets)
        {
            expr = subexpr;
            return true;
        }
        pos = tagContent.find_first_of(" \t\r\n)", pos + 1);
    }
    expr = tagContent.substr(0, pos);
    return pos != 0;
}


// Parse a tag into helper, expression and content
Handlebars::Tag
parseTag(
    std::string_view tagStr,
    std::string_view context)
{
    MRDOX_ASSERT(tagStr.size() >= 4);
    Handlebars::Tag t;
    t.escaped = tagStr.front() == '\\';
    MRDOX_ASSERT(tagStr[0 + t.escaped] == '{');
    MRDOX_ASSERT(tagStr[1 + t.escaped] == '{');
    MRDOX_ASSERT(tagStr[tagStr.size() - 1] == '}');
    MRDOX_ASSERT(tagStr[tagStr.size() - 2] == '}');
    t.buffer = tagStr;
    tagStr = tagStr.substr(2 + t.escaped, tagStr.size() - 4 - t.escaped);

    // ==============================================================
    // No HTML escape {{{ ... }}}
    // ==============================================================
    t.forceNoHTMLEscape = false;
    if (!tagStr.empty() && tagStr.front() == '{' && tagStr.back() == '}')
    {
        t.forceNoHTMLEscape = true;
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
        t.forceNoHTMLEscape = true;
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
        t.forceNoHTMLEscape = true;
        tagStr.remove_prefix(1);
        tagStr = trim_spaces(tagStr);
    }

    // ==============================================================
    // Tag type {{# ... }}, {{/ ... }}, {{^ ... }}, {{> ... }}, {{! ... }}
    // ==============================================================
    if (tagStr.starts_with('^')) {
        t.type = '^';
        tagStr.remove_prefix(1);
        tagStr = trim_spaces(tagStr);
        t.content = tagStr;
    } else if (tagStr.starts_with("else")) {
        t.type = '^';
        tagStr.remove_prefix(4);
        tagStr = trim_spaces(tagStr);
        t.content = tagStr;
    } else {
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
        } else {
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
    } else {
        t.helper = tagStr;
        t.arguments = {};
    }

    // ==============================================================
    // Check if tag is standalone
    // ==============================================================
    static constexpr std::array<char, 5> block_tag_types({'#', '^', '/', '>', '*'});
    bool const isBlock = std::ranges::find(
        block_tag_types, t.type) != block_tag_types.end();
    if (isBlock)
    {
        MRDOX_ASSERT(t.buffer.data() >= context.data());
        MRDOX_ASSERT(t.buffer.data() + t.buffer.size() <= context.data() + context.size());

        // Check if tag is standalone
        std::string_view beforeTag = context.substr(
            0, t.buffer.data() - context.data());
        auto posL = beforeTag.find_last_not_of(' ');
        bool const isStandaloneL =
            posL == std::string_view::npos || beforeTag[posL] == '\n';
        std::string_view afterTag = context.substr(
            t.buffer.data() + t.buffer.size() - context.data());
        auto posR = afterTag.find_first_not_of(' ');
        bool const isStandaloneR =
            posR == std::string_view::npos || afterTag[posR] == '\n';
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

void
Handlebars::
render_to(
    OutputRef& out,
    std::string_view templateText,
    dom::Value const& context,
    HandlebarsOptions const& options) const
{
    detail::RenderState state;
    state.templateText0 = templateText;
    state.templateText = templateText;
    if (options.data.isNull())
    {
        state.data.set("root", context);
        state.data.set("level", "warn");
    }
    else if (options.data.isObject())
    {
        state.data = options.data.getObject();
    }
    state.inlinePartials.emplace_back();
    state.compatStack.emplace_back(context);
    render_to(out, context, options, state);
}

void
Handlebars::
render_to(
    OutputRef& out,
    dom::Value const& context,
    HandlebarsOptions const& opt,
    detail::RenderState& state) const
{
    while (!state.templateText.empty()) {
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
        Tag tag = parseTag(tagStr, state.templateText0);

        // ==============================================================
        // Render template text before tag
        // ==============================================================
        std::string_view beforeTag = state.templateText.substr(0, tagStartPos - isDoubleEscaped);
        if (tag.removeLWhitespace) {
            beforeTag = trim_rspaces(beforeTag);
        }
        else if (!opt.ignoreStandalone && tag.isStandalone)
        {
            if (tag.type == '#' || tag.type == '^' || tag.type == '/')
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
        renderTag(tag, out, context, opt, state);

        // ==============================================================
        // Advance template text
        // ==============================================================
        if (tag.removeRWhitespace)
        {
            state.templateText = trim_lspaces(state.templateText);
        }
    }
}

std::string
JSON_stringify(
    dom::Value const& value,
    std::unordered_set<void*>& done) {
    switch(value.kind())
    {
    case dom::Kind::Null:
        return "null";
    case dom::Kind::Boolean:
        return value.getBool() ? "true" : "false";
    case dom::Kind::Integer:
        return std::to_string(value.getInteger());
    case dom::Kind::String:
    {
        std::string s = "\"";
        s += value.getString();
        s += "\"";
        return s;
    }
    case dom::Kind::Array:
    {
        dom::Array const& arr = value.getArray();
        if (done.find(arr.impl().get()) != done.end())
            return "[Circular]";
        done.insert(arr.impl().get());
        if (arr.empty())
            return "[]";
        std::string s = "[";
        {
            auto const n = arr.size();
            for(std::size_t i = 0; i < n; ++i)
            {
                if(i != 0)
                    s += ',';
                s += JSON_stringify(arr.at(i), done);
            }
        }
        s += "]";
        return s;
    }
    case dom::Kind::Object:
    {
        dom::Object const& obj = value.getObject();
        if (done.find(obj.impl().get()) != done.end())
            return "[Circular]";
        done.insert(obj.impl().get());
        if(obj.empty())
            return "{}";
        std::string s = "{";
        {
            for (std::size_t i = 0; i < obj.size(); ++i)
            {
                if (i != 0)
                    s += ',';
                dom::Object::reference item = obj.get(i);
                s += '"';
                s += item.key;
                s += "\":";
                std::string tmp = JSON_stringify(item.value, done);
                s += tmp;
            }
        }
        s += "}";
        return s;
    }
    default:
        MRDOX_UNREACHABLE();
    }
}

std::string
JSON_stringify(dom::Value const& value) {
    // Store visited pointers to avoid infinite recursion
    // similar to how JSON.stringify works
    std::unordered_set<void*> done;
    return JSON_stringify(value, done);
}

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
    if (expression.empty())
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
    default: return char(-1);
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
        } else {
            if (i + 1 < str.length()) {
                char c = unescapeChar(str[i + 1]);
                if (c != char(-1)) {
                    unescapedString.push_back('\\');
                    unescapedString.push_back(str[i + 1]);
                    ++i;
                } else {
                    unescapedString.push_back(c);
                }
            } else {
                unescapedString.push_back('\\');
            }
        }
    }
    return unescapedString;
}

std::string
appendContextPath(dom::Value const& contextPath, std::string_view id) {
    if (contextPath.isString()) {
        return std::string(contextPath.getString()) + "." + std::string(id);
    } else {
        return std::string(id);
    }
}

bool
popContextSegment(std::string_view& contextPath) {
    if (contextPath.empty())
        return false;
    auto pos = contextPath.find_last_of('.');
    if (pos == std::string_view::npos)
    {
        contextPath.remove_prefix(contextPath.size());
    }
    else
    {
        contextPath = contextPath.substr(0, pos);
    }
    return true;
}


std::pair<dom::Value, bool>
Handlebars::
evalExpr(
    dom::Value const & context,
    std::string_view expression,
    detail::RenderState& state,
    HandlebarsOptions const& opt,
    bool evalLiterals) const
{
    expression = trim_spaces(expression);
    if (evalLiterals)
    {
        if (is_literal_value(expression, "true"))
        {
            return {true, true};
        }
        if (is_literal_value(expression, "false"))
        {
            return {false, true};
        }
        if (is_literal_value(expression, "null") || is_literal_value(expression, "undefined") || expression.empty())
        {
            return {nullptr, true};
        }
        if (expression == "." || expression == "this")
        {
            return {context, true};
        }
        if (is_literal_string(expression))
        {
            return {unescapeString(expression), true};
        }
        if (is_literal_integer(expression))
        {
            std::int64_t value;
            auto res = std::from_chars(
                expression.data(),
                expression.data() + expression.size(),
                value);
            if (res.ec != std::errc())
                return {std::int64_t(0), true};
            return {value, true};
        }
        if (expression.starts_with('(') && expression.ends_with(')'))
        {
            std::string_view all = expression.substr(1, expression.size() - 2);
            std::string_view helper;
            findExpr(helper, all);
            auto [fn, found] = getHelper(helper, false);
            all.remove_prefix(helper.data() + helper.size() - all.data());
            dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
            HandlebarsCallback cb;
            cb.name_ = helper;
            cb.context_ = &context;
            setupArgs(all, context, state, args, cb, opt);
            return {fn(args, cb).first, true};
        }
    }
    if (expression.starts_with('@'))
    {
        expression.remove_prefix(1);
        return lookupPropertyImpl(state.data, expression, state);
    }
    if (expression.starts_with("..")) {
        auto contextPathV = state.data.find("contextPath");
        if (!contextPathV.isString())
            return {nullptr, false};
        std::string_view contextPath = contextPathV.getString();
        // Remove last segment if it's a literal integer
        if (!contextPath.empty())
        {
            auto pos = contextPath.find_last_of('.');
            if (pos != std::string_view::npos)
            {
                auto lastSegment = contextPath.substr(pos + 1);
                if (is_literal_integer(lastSegment))
                {
                    contextPath = contextPath.substr(0, pos);
                }
            }
        }
        while (expression.starts_with("..")) {
            if (!popContextSegment(contextPath))
                return {nullptr, false};
            expression.remove_prefix(2);
            if (!expression.empty()) {
                if (expression.front() != '/') {
                    return {nullptr, false};
                }
                expression.remove_prefix(1);
            }
        }
        std::string absContextPath;
        dom::Value root = state.data.find("root");
        do {
            absContextPath = contextPath;
            if (!expression.empty())
            {
                absContextPath += '.';
                absContextPath += expression;
            }
            auto [v, defined] = lookupPropertyImpl(root, absContextPath, state);
            if (defined) {
                return {v, defined};
            }
            popContextSegment(contextPath);
        } while (!contextPath.empty());
        return lookupPropertyImpl(root, expression, state);
    }
    auto [r, defined] = lookupPropertyImpl(context, expression, state);
    if (defined) {
        return {r, defined};
    }
    std::tie(r, defined) = lookupPropertyImpl(state.blockValues, expression, state);
    if (defined)
    {
        return {r, defined};
    }
    if (opt.compat)
    {
        auto parentContexts = std::ranges::views::reverse(state.compatStack);
        for (auto parentContext: parentContexts)
        {
            std::tie(r, defined) = lookupPropertyImpl(parentContext, expression, state);
            if (defined)
            {
                return {r, defined};
            }
        }
    }
    return {nullptr, false};
}

auto
Handlebars::
getHelper(std::string_view helper, bool isNoArgBlock) const
    -> std::pair<helper_type const&, bool>
{
    auto it = helpers_.find(helper);
    if (it != helpers_.end())
    {
        return {it->second, true};
    }
    helper = !isNoArgBlock ? "helperMissing" : "blockHelperMissing";
    it = helpers_.find(helper);
    MRDOX_ASSERT(it != helpers_.end());
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
    if (name == "@partial-block")
    {
        return {
            state.partialBlocks[state.partialBlockLevel - 1],
            true};
    }

    return { {}, false };
}

// Parse a block starting at templateText
bool
parseBlock(
    std::string_view blockName,
    Handlebars::Tag const& tag,
    HandlebarsOptions const& opt,
    detail::RenderState const& state,
    std::string_view &templateText,
    OutputRef &out,
    std::string_view &fnBlock,
    std::string_view &inverseBlocks,
    Handlebars::Tag &inverseTag)
{
    // ==============================================================
    // Initial blocks
    // ==============================================================
    fnBlock=templateText;
    inverseBlocks = {};
    if (!opt.ignoreStandalone && tag.isStandalone)
    {
        fnBlock = trim_ldelimiters(fnBlock, " ");
        if (fnBlock.starts_with('\n'))
        {
            fnBlock.remove_prefix(1);
        }
    }

    // ==============================================================
    // Iterate over the template to find tags and blocks
    // ==============================================================
    Handlebars::Tag closeTag;
    int l = 1;
    std::string_view* curBlock = &fnBlock;
    while (!templateText.empty())
    {
        // ==============================================================
        // Find next tag
        // ==============================================================
        std::string_view tagStr;
        if (!findTag(tagStr, templateText))
            break;

        Handlebars::Tag curTag = parseTag(tagStr, state.templateText0);

        // move template after the tag
        auto tag_pos = curTag.buffer.data() - templateText.data();
        templateText.remove_prefix(tag_pos + curTag.buffer.size());

        // ==============================================================
        // Update section level
        // ==============================================================
        if (!tag.rawBlock) {
            if (curTag.type == '#' || curTag.type2 == '#') {
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
                    if (closeTag.content != blockName)
                    {
                        auto res = find_position_in_text(state.templateText0, blockName);
                        std::string msg(blockName);
                        msg += " doesn't match ";
                        msg += closeTag.content;
                        if (res)
                        {
                            throw HandlebarsError(msg, res.line, res.column, res.pos);
                        }
                        throw HandlebarsError(msg);

                        return false;
                    }
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
#if 0
                    else if (!opt.ignoreStandalone && inverseTag.isStandalone)
                    {
                        *curBlock = trim_ldelimiters(*curBlock, " ");
                        if (curBlock->starts_with('\n'))
                        {
                            curBlock->remove_prefix(1);
                        }
                    }
#endif
                }
            }
        } else {
            // ==============================================================
            // Raw blocks
            // ==============================================================
            if (curTag.type == '/' && tag.rawBlock == curTag.rawBlock && blockName == curTag.content) {
                // Closing the raw section: l = 0;
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
    // Apply close tag whitespace control
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
    }

    return true;
}


// Render a handlebars tag
void
Handlebars::
renderTag(
    Tag const& tag,
    OutputRef& out,
    dom::Value const& context,
    HandlebarsOptions const& opt,
    detail::RenderState& state) const {
    if (tag.type == '#' || tag.type == '^')
    {
        renderBlock(tag.helper, tag, out, context, opt, state, false);
    }
    else if (tag.type == '>')
    {
        renderPartial(tag, out, context, opt, state);
    }
    else if (tag.type == '*')
    {
        renderDecorator(tag, out, context, opt, state);
    }
    else if (tag.type != '/' && tag.type != '!')
    {
        renderExpression(tag, out, context, opt, state);
    }
}

void
Handlebars::
renderExpression(
    Handlebars::Tag const &tag,
    OutputRef &out,
    dom::Value const & context,
    HandlebarsOptions const &opt,
    detail::RenderState& state) const
{
    if (tag.helper.empty())
        return;

    auto opt2 = opt;
    opt2.noEscape = tag.forceNoHTMLEscape || opt.noEscape;

    // Evaluate helper as function
    auto it = helpers_.find(tag.helper);
    if (it != helpers_.end()) {
        auto [fn, found] = getHelper(tag.helper, false);
        dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
        HandlebarsCallback cb;
        cb.name_ = tag.helper;
        cb.context_ = &context;
        cb.data_ = &state.data;
        cb.logger_ = &logger_;
        setupArgs(tag.arguments, context, state, args, cb, opt);
        auto [res, render] = fn(args, cb);
        if (render == HelperBehavior::RENDER_RESULT) {
            format_to(out, res, opt2);
        } else if (render == HelperBehavior::RENDER_RESULT_NOESCAPE) {
            opt2.noEscape = true;
            format_to(out, res, opt2);
        }
        if (tag.removeRWhitespace) {
            state.templateText = trim_lspaces(state.templateText);
        }
        return;
    }

    // Evaluate helper as expression
    std::string_view helper_expr = tag.helper;
    std::string unescaped;
    if (is_literal_string(tag.helper))
    {
        unescaped = unescapeString(helper_expr);
        helper_expr = unescaped;
    }
    auto [v, defined] = evalExpr(context, helper_expr, state, opt, false);
    if (defined)
    {
        format_to(out, v, opt2);
        return;
    }

    // Let it decay to helperMissing hook
    auto [fn, found] = getHelper(helper_expr, false);
    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    HandlebarsCallback cb;
    cb.name_ = helper_expr;
    setupArgs(tag.arguments, context, state, args, cb, opt);
    auto [res, render] = fn(args, cb);
    if (render == HelperBehavior::RENDER_RESULT) {
        format_to(out, res, opt2);
    } else if (render == HelperBehavior::RENDER_RESULT_NOESCAPE) {
        opt2.noEscape = true;
        format_to(out, res, opt2);
    }
    if (tag.removeRWhitespace) {
        state.templateText = trim_lspaces(state.templateText);
    }
}

void
Handlebars::
setupArgs(
    std::string_view expression,
    dom::Value const& context,
    detail::RenderState & state,
    dom::Array &args,
    HandlebarsCallback& cb,
    HandlebarsOptions const& opt) const
{
    std::string_view expr;
    while (findExpr(expr, expression))
    {
        expression = expression.substr(expr.data() + expr.size() - expression.data());
        auto [k, v] = findKeyValuePair(expr);
        if (k.empty())
        {
            args.emplace_back(evalExpr(context, expr, state, opt, true).first);
            cb.ids_.push_back(expr);
        }
        else
        {
            cb.hashes_.set(k, evalExpr(context, v, state, opt, true).first);
        }
    }
    cb.renderState_ = &state;
}

void
Handlebars::
renderDecorator(
    Handlebars::Tag const& tag,
    OutputRef &out,
    dom::Value const& context,
    HandlebarsOptions const& opt,
    detail::RenderState& state) const {
    // Validate decorator
    if (tag.helper != "inline")
    {
        out << fmt::format(R"([undefined decorator "{}" in "{}"])", tag.helper, tag.buffer);
        return;
    }

    // Evaluate expression
    std::string_view expr;
    findExpr(expr, tag.arguments);
    auto [value, defined] = evalExpr(context, expr, state, opt, true);
    if (!value.isString())
    {
        out << fmt::format(R"([invalid decorator expression "{}" in "{}"])", tag.arguments, tag.buffer);
        return;
    }
    std::string_view partial_name = value.getString();

    // Parse block
    std::string_view fnBlock;
    std::string_view inverseBlock;
    Tag inverseTag;
    if (tag.type2 == '#') {
        if (!parseBlock(tag.helper, tag, opt, state, state.templateText, out, fnBlock, inverseBlock, inverseTag)) {
            return;
        }
    }
    fnBlock = trim_rspaces(fnBlock);
    state.inlinePartials.back()[std::string(partial_name)] = fnBlock;
}

void
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
        auto [value, defined] = evalExpr(context, expr, state, opt, true);
        if (value.isString())
        {
            partialName = value.getString();
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
        if (!parseBlock(tag.helper, tag, opt, state, state.templateText, out, fnBlock, inverseBlock, inverseTag))
        {
            return;
        }
    }

    // ==============================================================
    // Find partial content
    // ==============================================================
    // Look for registered partial
    auto [partial_content, found] = getPartial(partialName, state);
    if (!found)
    {
        if (tag.type2 == '#')
        {
            partial_content = fnBlock;
        }
        else
        {
            throw HandlebarsError(fmt::format("The partial {} could not be found",
                                              partialName));
        }
    }

    // ==============================================================
    // Evaluate partial block to extract inline partials
    // ==============================================================
    if (tag.type2 == '#')
    {
        state.inlinePartials.emplace_back();
        OutputRef dumb{};
        std::string_view templateText = state.templateText;
        state.templateText = fnBlock;
        this->render_to(dumb, context, opt, state);
        state.templateText = templateText;
    }

    // ==============================================================
    // Set @partial-block
    // ==============================================================
    if (tag.type2 == '#')
    {
        state.partialBlocks.emplace_back(fnBlock);
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

    // Populate with arguments
    if (!tag.arguments.empty())
    {
        // create context from specified keys
        auto tagContent = tag.arguments;
        bool partialCtxDefined = false;
        std::string_view expr;
        while (findExpr(expr, tagContent))
        {
            tagContent = tagContent.substr(expr.data() + expr.size() - tagContent.data());
            auto [partialKey, contextKey] = findKeyValuePair(expr);
            bool const isContextReplacement = partialKey.empty();
            if (isContextReplacement)
            {
                // Check if context has been replaced before
                if (partialCtxDefined)
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
                    std::string msg = fmt::format(
                        "Unsupported number of partial arguments: {}", n);
                    auto res = find_position_in_text(state.templateText0, tag.buffer);
                    if (res)
                    {
                        throw HandlebarsError(msg, res.line, res.column, res.pos);
                    }
                    throw HandlebarsError(msg);
                }

                // Replace context
                auto [value, defined] = evalExpr(context, expr, state, opt, true);
                if (defined)
                {
                    if (value.isObject())
                    {
                        partialCtx = createFrame(value.getObject());
                    }
                    else
                    {
                        partialCtx = value;
                    }
                }
                partialCtxDefined = true;
                continue;
            }

            // Argument is key=value pair
            dom::Value value;
            bool defined;
            if (contextKey != ".")
            {
                std::tie(value, defined) = evalExpr(context, contextKey, state, opt, true);
            }
            else
            {
                value = context;
                defined = true;
            }
            if (defined)
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
                partialCtx.getObject().set(partialKey, value);
            }
        }
    }

    // ==============================================================
    // Render partial
    // ==============================================================
    // Partial state
    std::string_view templateText0 = state.templateText0;
    state.templateText0 = partial_content;
    std::string_view templateText = state.templateText;
    state.templateText = partial_content;
    bool const isPartialBlock = partialName == "@partial-block";
    state.partialBlockLevel -= isPartialBlock;
    out.setIndent(out.getIndent() + tag.standaloneIndent * !opt.preventIndent);
    state.compatStack.emplace_back(context);
    // Render
    this->render_to(out, partialCtx, opt, state);
    // Restore state
    state.compatStack.pop_back();
    out.setIndent(out.getIndent() - tag.standaloneIndent * !opt.preventIndent);
    state.partialBlockLevel += isPartialBlock;
    state.templateText = templateText;
    state.templateText0 = templateText0;

    if (tag.type2 == '#')
    {
        state.inlinePartials.pop_back();
        state.partialBlocks.pop_back();
        --state.partialBlockLevel;
    }

    // ==============================================================
    // Remove standalone whitespace
    // ==============================================================
    if (!opt.ignoreStandalone && tag.isStandalone)
    {
        state.templateText = trim_ldelimiters(state.templateText, " ");
        if (state.templateText.starts_with('\n'))
        {
            state.templateText.remove_prefix(1);
        }
    }
}

void
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
    parseBlock(blockName, tag, opt, state, state.templateText, out, fnBlock, inverseBlock, inverseTag);

    // ==============================================================
    // Setup helper parameters
    // ==============================================================
    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    HandlebarsCallback cb;
    cb.name_ = tag.helper;
    cb.context_ = &context;
    cb.data_ = &state.data;
    cb.logger_ = &logger_;
    cb.output_ = &out;
    bool const isNoArgBlock = tag.arguments.empty();
    auto [fn, found] = getHelper(tag.helper, isNoArgBlock);
    std::string_view arguments = tag.arguments;
    bool const emulateMustache = !found && isNoArgBlock;
    std::string unescaped;
    if (emulateMustache) {
        if (is_literal_string(tag.helper))
        {
            unescaped = unescapeString(tag.helper);
            arguments = unescaped;
        }
        else
        {
            arguments = tag.helper;
        }
    }
    setupArgs(arguments, context, state, args, cb, opt);

    // ==============================================================
    // Setup block parameters
    // ==============================================================
    std::string_view expr;
    std::string_view bps = tag.blockParams;
    while (findExpr(expr, bps))
    {
        bps = bps.substr(expr.data() + expr.size() - bps.data());
        cb.blockParams_.push_back(expr);
    }

    // ==============================================================
    // Setup callbacks
    // ==============================================================
    if (!tag.rawBlock) {
        cb.fn_ = [this, fnBlock, opt, &state](
            OutputRef os,
            dom::Value const &item,
            dom::Object const &data,
            dom::Object const &newBlockValues) -> void
        {
            std::string_view templateText = state.templateText;
            state.templateText = fnBlock;
            dom::Object state_data = state.data;
            state.data = dom::Object(data);
            if (!newBlockValues.empty()) {
                dom::Object blockValuesOverlay =
                    createFrame(newBlockValues, state.blockValues);
                dom::Object blockValues = state.blockValues;
                state.blockValues = std::move(blockValuesOverlay);
                render_to(os, item, opt, state);
                state.blockValues = std::move(blockValues);
            } else {
                render_to(os, item, opt, state);
            }
            state.templateText = templateText;
            state.data = std::move(state_data);
        };
        cb.inverse_ =
            [this, inverseTag, inverseBlock, opt, blockName, &state](
            OutputRef os,
            dom::Value const &item,
            dom::Object const &data,
            dom::Object const &newBlockValues) -> void
        {
            std::string_view templateText = state.templateText;
            state.templateText = inverseBlock;
            dom::Object state_data = state.data;
            state.data = dom::Object(data);
            if (inverseTag.helper.empty()) {
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
                }
                render_to(os, item, opt, state);
            }
            else
            {
                if (!newBlockValues.empty()) {
                    dom::Object blockValuesOverlay =
                        createFrame(newBlockValues, state.blockValues);
                    dom::Object blockValues = state.blockValues;
                    state.blockValues = std::move(blockValuesOverlay);
                    renderBlock(blockName, inverseTag, os, item, opt, state, true);
                    state.blockValues = std::move(blockValues);
                } else {
                    renderBlock(blockName, inverseTag, os, item, opt, state, true);
                }
            }
            state.templateText = templateText;
            state.data = std::move(state_data);
        };
    } else {
        cb.fn_ = [fnBlock](
            OutputRef os,
            dom::Value const & /* item */,
            dom::Object const & /* data */,
            dom::Object const & /* newBlockValues */) -> void {
            // Render raw fnBlock
            os << fnBlock;
        };
        cb.inverse_ = [](
            OutputRef /* os */,
            dom::Value const &/* item */,
            dom::Object const &/* data */,
            dom::Object const &/* newBlockValues */) -> void {
            // noop: No inverseBlock for raw block
        };
    }

    bool const isStandaloneInvertedSection = tag.type == '^' && !isChainedBlock;
    if (isStandaloneInvertedSection)
    {
        std::swap(cb.fn_, cb.inverse_);
    }

    // ==============================================================
    // Call helper
    // ==============================================================
    state.inlinePartials.emplace_back();
    state.compatStack.emplace_back(context);
    auto [res, render] = fn(args, cb);
    if (render == HelperBehavior::RENDER_RESULT) {
        format_to(out, res, opt);
    } else if (render == HelperBehavior::RENDER_RESULT_NOESCAPE) {
        HandlebarsOptions opt2 = opt;
        opt2.noEscape = true;
        format_to(out, res, opt2);
    }
    state.inlinePartials.pop_back();
    state.compatStack.pop_back();
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
unregisterHelper(std::string_view name) {
    auto it = helpers_.find(name);
    if (it != helpers_.end())
        helpers_.erase(it);
    // Re-register mandatory helpers
    if (name == "helperMissing")
        registerHelper("helperMissing", helpers::helper_missing_fn);
    else if (name == "blockHelperMissing")
        registerHelper("blockHelperMissing", helpers::block_helper_missing_fn);
}

void
Handlebars::
registerHelperImpl(
    std::string_view name,
    helper_type const &helper)
{
    auto it = helpers_.find(name);
    if (it != helpers_.end())
        helpers_.erase(it);
    helpers_.emplace(std::string(name), helper);
}

void
Handlebars::
registerLogger(
    std::function<void(dom::Value, dom::Array const&)> fn)
{
    logger_ = std::move(fn);
}

namespace helpers {

void
registerBuiltinHelpers(Handlebars& hbs)
{
    hbs.registerHelper("if", if_fn);
    hbs.registerHelper("unless", unless_fn);
    hbs.registerHelper("with", with_fn);
    hbs.registerHelper("each", each_fn);
    hbs.registerHelper("lookup", lookup_fn);
    hbs.registerHelper("log", log_fn);
    hbs.registerHelper("helperMissing", helper_missing_fn);
    hbs.registerHelper("blockHelperMissing", block_helper_missing_fn);
}

void
registerAntoraHelpers(Handlebars& hbs)
{
    hbs.registerHelper("and", and_fn);
    hbs.registerHelper("detag", detag_fn);
    hbs.registerHelper("eq", eq_fn);
    hbs.registerHelper("increment", increment_fn);
    hbs.registerHelper("ne", ne_fn);
    hbs.registerHelper("not", not_fn);
    hbs.registerHelper("or", or_fn);
    hbs.registerHelper("relativize", relativize_fn);
    hbs.registerHelper("year", year_fn);
}

bool
and_fn(dom::Array const& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!isTruthy(args[i])) {
            return false;
        }
    }
    return true;
}

bool
or_fn(dom::Array const& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (isTruthy(args[i])) {
            return true;
        }
    }
    return false;
}

bool
isSame(dom::Value const& a, dom::Value const& b);

bool
isSame(dom::Object const& a, dom::Object const& b) {
    if (a.size() != b.size()) {
        return false;
    }
    return std::ranges::all_of(a, [&b](const auto & i) {
        std::string_view k = i.key;
        if (!b.exists(k)) {
            return false;
        }
        if (!isSame(i.value, b.find(k))) {
            return false;
        }
        return true;
    });
}

bool
isSame(dom::Array const& a, dom::Array const& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!isSame(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

bool
isSame(dom::Value const& a, dom::Value const& b) {
    if (a.kind() != b.kind()) {
        return false;
    }
    switch (a.kind()) {
        case dom::Kind::Null:
            return true;
        case dom::Kind::Boolean:
            return a.getBool() == b.getBool();
        case dom::Kind::Integer:
            return a.getInteger() == b.getInteger();
        case dom::Kind::String:
            return a.getString() == b.getString();
        case dom::Kind::Array:
            return isSame(a.getArray(), b.getArray());
        case dom::Kind::Object:
            return isSame(a.getObject(), b.getObject());
        case dom::Kind::Function:
            return isSame(a.getFunction(), b.getFunction());
    }
    return false;
}

bool
isLt(dom::Value const& a, dom::Value const& b);

bool
isLt(dom::Object const& a, dom::Object const& b) {
    if (a.size() < b.size()) {
        return true;
    }
    if (b.size() < a.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto refa = a.get(i);
        auto refb = b.get(i);
        if (isLt(refa.key, refb.key)) {
            return true;
        }
        if (isLt(refb.key, refa.key)) {
            return false;
        }
        if (isLt(refa.value, refb.value)) {
            return true;
        }
        if (isLt(refb.value, refa.value)) {
            return false;
        }
    }
    return false;
}

bool
isLt(dom::Array const& a, dom::Array const& b) {
    if (a.size() < b.size()) {
        return true;
    }
    if (b.size() < a.size()) {
        return true;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (isLt(a[i], b[i])) {
            return true;
        }
        if (isLt(b[i], a[i])) {
            return false;
        }
    }
    return false;
}

bool
isLt(dom::Value const& a, dom::Value const& b) {
    using kind_t = std::underlying_type_t<dom::Kind>;
    if (static_cast<kind_t>(a.kind()) < static_cast<kind_t>(b.kind())) {
        return true;
    }
    if (static_cast<kind_t>(b.kind()) < static_cast<kind_t>(a.kind())) {
        return false;
    }
    switch (a.kind()) {
        case dom::Kind::Null:
            return false;
        case dom::Kind::Boolean:
            return !a.getBool() && b.getBool();
        case dom::Kind::Integer:
            return a.getInteger() < b.getInteger();
        case dom::Kind::String:
            return a.getString().get() < b.getString().get();
        case dom::Kind::Array:
            return isLt(a.getArray(), b.getArray());
        case dom::Kind::Object:
            return isLt(a.getObject(), b.getObject());
        case dom::Kind::Function:
            return true;
    }
    return false;
}

bool
eq_fn(dom::Array const& args) {
    if (args.empty()) {
        return true;
    }
    dom::Value first = args[0];
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (!isSame(first, args[i])) {
            return false;
        }
    }
    return true;
}

bool
ne_fn(dom::Array const& args) {
    return !eq_fn(args);
}

std::string_view
kindToString(dom::Kind kind) {
    switch (kind) {
        case dom::Kind::Null:
            return "null";
        case dom::Kind::Object:
            return "object";
        case dom::Kind::Array:
            return "array";
        case dom::Kind::String:
            return "string";
        case dom::Kind::Integer:
            return "integer";
        case dom::Kind::Boolean:
            return "boolean";
        default:
            MRDOX_UNREACHABLE();
    }
}

std::string
validateArgs(
    std::string_view helper,
    std::size_t n,
    dom::Array const& args) {
    if (args.size() != n) {
        return fmt::format(R"(["{}" helper requires {} arguments: {} arguments provided])", helper, n, args.size());
    }
    return {};
}

std::string
validateArgs(
    std::string_view helper,
    std::initializer_list<dom::Kind> il,
    dom::Array const& args) {
    auto r = validateArgs(helper, il.size(), args);
    if (!r.empty()) {
        return r;
    }
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i].kind() != il.begin()[i]) {
            return fmt::format(R"(["{}" helper requires argument {} to be of type {}: {} provided])", helper, i, kindToString(il.begin()[i]), kindToString(args[i].kind()));
        }
    }
    return {};
}

bool
not_fn(dom::Array const& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!isTruthy(args[i])) {
            return true;
        }
    }
    return false;
}

dom::Value
increment_fn(
    dom::Array const &args,
    HandlebarsCallback const& options)
{
    auto r = validateArgs(options.name(), 1, args);
    if (!r.empty()) {
        return r;
    }
    dom::Value arg = args[0];
    if (arg.kind() == dom::Kind::Integer)
        return arg.getInteger() + 1;
    if (arg.kind() == dom::Kind::Boolean)
        return true;
    else
        return arg;
}

std::string
detag_fn(
    dom::Array const& args,
    HandlebarsCallback const& options)
{
    auto r = validateArgs(options.name(), {dom::Kind::String}, args);
    if (!r.empty()) {
        return r;
    }
    std::string html(args[0].getString());
    std::string result;
    result.reserve(html.size());
    bool insideTag = false;
    for (char c : html) {
        if (c == '<') {
            insideTag = true;
            continue;
        }
        if (c == '>') {
            insideTag = false;
            continue;
        }
        if (!insideTag) {
            result.push_back(c);
        }
    }
    return result;
}

dom::Value
relativize_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    if (args.empty()) {
        return "#";
    }

    // Destination path
    if (!args[0].isString()) {
        return fmt::format(R"(["relativize" helper requires a string argument: {} provided])", kindToString(args[0].kind()));
    }
    std::string_view to = args[0].getString();
    if (!isTruthy(to)) {
        return "#";
    }
    if (to.front() != '/') {
        return to;
    }

    // Source path
    std::string_view from;
    dom::Value ctx = options.context();
    if (args.size() > 1)
    {
        if (args[1].kind() == dom::Kind::String)
            from = args[1].getString();
        else if (args[1].kind() == dom::Kind::Object)
            ctx = args[1];
    }

    if (from.empty() && ctx.isObject()) {
        auto [v, defined] = options.lookupProperty(ctx, "data.root.page");
        if (v.isString()) {
            from = v.getString();
        } else {
            std::tie(v, defined) = options.lookupProperty(ctx, "data.root.site.path");
            if (v.isString()) {
                std::string fromStr(v.getString());
                fromStr += to;
                return fromStr;
            }
        }
    }

    auto hashIdx = std::ranges::find(to, '#') - to.begin();
    std::string_view hash = to.substr(hashIdx);
    to = to.substr(0, hashIdx);
    if (to == from)
    {
        if (!hash.empty())
        {
            return hash;
        }
        if (files::isDirsy(to))
        {
            return "./";
        }
        return {files::getFileName(to)};
    }
    else
    {
        // AFREITAS: Implement this functionality without std::filesystem
        std::string lhs = std::filesystem::relative(
            std::filesystem::path(std::string_view(to)),
            std::filesystem::path(std::string_view(from))).generic_string();
        if (lhs.empty())
        {
            lhs = ".";
        }
        if (files::isDirsy(to))
        {
            lhs += '/';
        }
        lhs += hash;
        return lhs;
    }
}

int
year_fn() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&currentTime);
    return localTime->tm_year + 1900;
}

void
if_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    OutputRef out = options.output();
    auto r = validateArgs(options.name(), 1, args);
    if (!r.empty()) {
        out << r;
        return;
    }

    dom::Value const& conditional = args[0];
    if (conditional.kind() == dom::Kind::Integer) {
        // Treat the zero path separately
        std::int64_t v = conditional.getInteger();
        if (v == 0) {
            bool includeZero = false;
            if (options.hashes().exists("includeZero")) {
                auto zeroV = options.hashes().find("includeZero");
                if (zeroV.isBoolean()) {
                    includeZero = zeroV.getBool();
                }
            }
            if (includeZero) {
                options.fn(out);
            } else {
                options.inverse(out);
            }
            return;
        }
    }
    if (isTruthy(conditional)) {
        options.fn(out);
        return;
    }
    options.inverse(out);
}

void
unless_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    OutputRef out = options.output();
    auto r = validateArgs(options.name(), 1, args);
    if (!r.empty()) {
        out << r;
        return;
    }

    dom::Value const& conditional = args[0];
    if (conditional.kind() == dom::Kind::Integer) {
        // Treat the zero path separately
        std::int64_t v = conditional.getInteger();
        if (v == 0) {
            bool includeZero = false;
            if (options.hashes().exists("includeZero")) {
                auto zeroV = options.hashes().find("includeZero");
                if (zeroV.isBoolean()) {
                    includeZero = zeroV.getBool();
                }
            }
            if (includeZero) {
                options.inverse(out);
            } else {
                options.fn(out);
            }
            return;
        }
    }
    if (isTruthy(conditional)) {
        options.inverse(out);
        return;
    }
    options.fn(out);
}

void
with_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    OutputRef out = options.output();
    auto r = validateArgs(options.name(), 1, args);
    if (!r.empty()) {
        out << r;
        return;
    }
    dom::Value newContext = args[0];
    if (!isEmpty(newContext)) {
        dom::Object data = createFrame(options.data());
        std::string contextPath = appendContextPath(
            options.data().find("contextPath"),
            options.ids()[0]);
        data.set("contextPath", contextPath);
        dom::Object blockValues;
        if (!options.blockParams().empty()) {
            blockValues.set(options.blockParams()[0], newContext);
        }
        options.fn(out, newContext, data, blockValues);
        return;
    }
    options.inverse(out);
}

void
each_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    OutputRef out = options.output();
    auto r = validateArgs(options.name(), 1, args);
    if (!r.empty()) {
        out << r;
        return;
    }

    MRDOX_ASSERT(!options.ids().empty());
    std::string contextPath = appendContextPath(
        options.data().find("contextPath"), options.ids()[0]) + '.';

    dom::Object data = createFrame(options.data());
    dom::Object blockValues;

    dom::Value context = args[0];
    std::size_t index = 0;
    if (context.isArray()) {
        dom::Array const& items = context.getArray();
        for (index = 0; index < items.size(); ++index) {
            dom::Value item = items.at(index);
            data.set("key", static_cast<std::int64_t>(index));
            data.set("index", static_cast<std::int64_t>(index));
            data.set("first", index == 0);
            data.set("last", index == items.size() - 1);
            data.set("contextPath", contextPath + std::to_string(index));
            if (!options.blockParams().empty()) {
                blockValues.set(options.blockParams()[0], item);
                if (options.blockParams().size() > 1) {
                    blockValues.set(
                        options.blockParams()[1], static_cast<std::int64_t>(index));
                }
            }
            options.fn(out, item, data, blockValues);
        }
    } else if (context.kind() == dom::Kind::Object) {
        dom::Object const& items = context.getObject();
        for (index = 0; index < items.size(); ++index) {
            dom::Object::reference item = items[index];
            data.set("key", item.key);
            data.set("index", static_cast<std::int64_t>(index));
            data.set("first", index == 0);
            data.set("last", index == items.size() - 1);
            data.set("contextPath", contextPath + std::string(item.key));
            if (!options.blockParams().empty()) {
                blockValues.set(options.blockParams()[0], item.value);
                if (options.blockParams().size() > 1) {
                    blockValues.set(options.blockParams()[1], item.key);
                }
            }
            options.fn(out, item.value, data, blockValues);
        }
    }
    if (index == 0) {
        options.inverse(out);
    }
}

dom::Value
lookup_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    auto r = validateArgs(options.name(), 2, args);
    if (!r.empty()) {
        throw std::runtime_error(r);
    }
    dom::Value obj = args[0];
    dom::Value field = args[1];
    if (!isTruthy(obj)) {
        return obj;
    }
    return options.lookupProperty(obj, field).first;
}

void
log_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    dom::Value level = 1;
    if (auto hl = options.hashes().find("level"); !hl.isNull()) {
        level = hl;
    } else if (auto ol = options.hashes().find("level"); !ol.isNull()) {
        level = ol;
    }
    options.log(level, args);
}

void
helper_missing_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    if (!args.empty()) {
        std::string msg = fmt::format(R"(Missing helper: "{}")", options.name());
        throw std::runtime_error(msg);
    }
}

void
block_helper_missing_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    OutputRef out = options.output();
    auto r = validateArgs(options.name(), 1, args);
    if (!r.empty()) {
        out << r;
        return;
    }
    dom::Value context = args[0];
    if (context.isBoolean()) {
        if (context.getBool()) {
            options.fn(out);
            return;
        } else {
            options.inverse(out);
            return;
        }
    } else if (context.isNull()) {
        options.inverse(out);
        return;
    } else if (context.isArray()) {
        // If the context is an array, then we'll iterate over it, rendering
        // the block for each item in the array like mustache does.
        dom::Array const& items = context.getArray();
        if (items.empty()) {
            options.inverse(out);
            return;
        }
        dom::Object data = createFrame(options.data());
        std::string contextPath = appendContextPath(
            options.data().find("contextPath"), options.ids()[0]) + '.';
        for (std::size_t index = 0; index < items.size(); ++index) {
            dom::Value item = items.at(index);
            data.set("key", static_cast<std::int64_t>(index));
            data.set("index", static_cast<std::int64_t>(index));
            data.set("first", index == 0);
            data.set("last", index == items.size() - 1);
            data.set("contextPath", contextPath + std::to_string(index));
            options.fn(out, item, data, {});
        }
    } else {
        // If the context is not an array, then we'll render the block once
        // with the context as the data.
        dom::Object data = createFrame(options.data());
        data.set("contextPath", appendContextPath(data.find("contextPath"), options.name()));
        options.fn(out, context, data, {});
    }
}

void
noop_fn(
    dom::Array const& args,
    HandlebarsCallback const& options) {
    OutputRef out = options.output();
    if (options.isBlock()) {
        // If the hook is not overridden, then the default implementation will
        // mimic the behavior of Mustache and just render the block.
        options.fn(out);
        return;
    }
    if (!args.empty()) {
        out << fmt::format(R"(Missing helper: "{}")", options.name());
    }
}

constexpr
std::int64_t
normalize_index(std::int64_t i, std::int64_t n) {
    if (i < 0 || i > n) {
        return (i % n + n) % n;
    }
    return i;
}

dom::Value
at_fn(dom::Array const& args, HandlebarsCallback const& options) {
    dom::Value range;
    dom::Value field;
    std::int64_t index = 0;
    if (options.isBlock()) {
        range = options.fn();
        field = args.at(0);
    } else {
        range = args.at(0);
        field = args.at(1);
    }
    if (field.isInteger()) {
        index = field.getInteger();
    }
    if (range.isString()) {
        std::string str(range.getString().get());
        index = normalize_index(index, static_cast<std::int64_t>(str.size()));
        return std::string(1, str.at(index));
    } else if (range.isArray()) {
        dom::Array const& arr = range.getArray();
        index = normalize_index(index, static_cast<std::int64_t>(arr.size()));
        return arr.at(index);
    } else if (range.isObject()) {
        dom::Object const& obj = range.getObject();
        if (!field.isString()) {
            return nullptr;
        }
        std::string key(field.getString().get());
        if (obj.exists(key)) {
            return obj.find(key);
        }
        return nullptr;
    } else {
        return range;
    }
}

dom::Value
concat_fn(dom::Array const& args, HandlebarsCallback const& options) {
    if (args.at(0).isString()) {
        std::string str;
        std::string sep;
        std::string str2;
        if (options.isBlock()) {
            str = options.fn();
            sep = args.at(0).getString();
            str2 = args.at(1).getString();
        } else {
            str = args.at(0).getString();
            sep = args.at(1).getString();
            str2 = args.at(2).getString();
        }
        return str + sep + str2;
    } else if (args.at(0).isArray()) {
        dom::Array res;
        for (std::size_t i = 0; i < args.size(); ++i) {
            dom::Value cur = args.at(i);
            auto const& curArray = cur.getArray();
            for (std::size_t j = 0; j < curArray.size(); ++j) {
                res.emplace_back(curArray.at(j));
            }
        }
        return res;
    } else {
        return args.at(0);
    }
}

std::int64_t
count_fn(dom::Array const& args, HandlebarsCallback const& options) {
    bool const stringOverload =
        (options.isBlock() && args.at(0).isString()) ||
        (args.at(0).isString() && args.at(1).isString());
    if (stringOverload) {
        std::string str;
        std::string sub;
        std::int64_t start = 0;
        std::int64_t end = 0;
        if (options.isBlock()) {
            str = options.fn();
            sub = args.at(0).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 1) {
                start = args.at(1).getInteger();
                if (args.size() > 2) {
                    end = args.at(2).getInteger();
                }
            }
        } else {
            str = args.at(0).getString();
            sub = args.at(1).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 2) {
                start = args.at(2).getInteger();
                if (args.size() > 3) {
                    end = args.at(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::int64_t count = 0;
        for (std::int64_t pos = start; pos < end; ++pos) {
            if (str.compare(pos, sub.size(), sub) == 0) {
                ++count;
            }
        }
        return count;
    } else {
        dom::Value range = args.at(0);
        dom::Value item = args.at(1);
        if (range.isString()) {
            std::string str(range.getString().get());
            std::int64_t count = 0;
            for (char i : str) {
                if (i == item.getString().get().at(0)) {
                    ++count;
                }
            }
            return count;
        } else if (range.isArray()) {
            dom::Array const& arr = range.getArray();
            std::int64_t count = 0;
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (isSame(arr.at(i), item)) {
                    ++count;
                }
            }
            return count;
        } else if (range.isObject()) {
            dom::Object const& obj = range.getObject();
            std::int64_t count = 0;
            for (auto const& [key, val] : obj) {
                if (isSame(val, item)) {
                    ++count;
                }
            }
            return count;
        } else {
            return 0;
        }
    }
}

dom::Value
replace_fn(dom::Array const& args, HandlebarsCallback const& options) {
    bool const stringOverload =
        (options.isBlock() && args.at(0).isString()) ||
        (args.at(0).isString() && args.at(1).isString());
    if (stringOverload) {
        std::string str;
        std::string old;
        std::string new_str;
        std::int64_t count = -1;
        if (options.isBlock()) {
            str = options.fn();
            old = args.at(0).getString();
            new_str = args.at(1).getString();
            if (args.size() > 2)
            count = args.at(2).getInteger();
        } else {
            str = args.at(0).getString();
            old = args.at(1).getString();
            new_str = args.at(2).getString();
            if (args.size() > 3)
            count = args.at(3).getInteger();
        }
        std::string res;
        std::size_t pos = 0;
        std::size_t old_len = old.size();
        while (count != 0) {
            std::size_t next = str.find(old, pos);
            if (next == std::string::npos) {
                res += str.substr(pos);
                break;
            }
            res += str.substr(pos, next - pos);
            res += new_str;
            pos = next + old_len;
            if (count > 0) {
                --count;
            }
        }
        return res;
    }
    dom::Value range = args.at(0);
    dom::Value item = args.at(1);
    dom::Value replacement = args.at(2);
    if (range.isString()) {
        std::string str(range.getString().get());
        for (std::int64_t i = 0; i < static_cast<std::int64_t>(str.size()); ++i) {
            if (str.at(i) == item.getString().get().at(0)) {
                str.replace(
                    str.begin() + i,
                    str.begin() + i + 1,
                    replacement.getString().get());
                i += static_cast<std::int64_t>(replacement.getString().get().size()) - 1;
            }
        }
        return str;
    } else if (range.isArray()) {
        dom::Array const& arr = range.getArray();
        dom::Array res;
        for (std::size_t i = 0; i < arr.size(); ++i) {
            dom::Value v = arr.at(i);
            if (isSame(v, item)) {
                res.emplace_back(replacement);
            } else {
                res.emplace_back(v);
            }
        }
        return res;
    } else if (range.isObject()) {
        dom::Object obj = createFrame(range.getObject());
        for (auto const& [key, val] : obj) {
            if (isSame(val, item)) {
                obj.set(key, replacement);
            }
        }
        return obj;
    } else {
        return range;
    }
}

void
registerStringHelpers(Handlebars& hbs)
{
    static constexpr auto toupper = [](char c) -> char {
        return static_cast<char>(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
    };

    static constexpr auto tolower = [](char c) -> char {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    };

    hbs.registerHelper("capitalize", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        if (!res.empty()) {
            res[0] = toupper(res[0]);
        }
        return res;
    });

    hbs.registerHelper("center", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        std::int64_t width = 0;
        char fillchar = ' ';
        if (options.isBlock()) {
            res = options.fn();
            width = args.at(0).getInteger();
            if (args.size() > 1) {
                fillchar = args.at(1).getString().get()[0];
            }
        } else {
            res = args.at(0).getString();
            width = args.at(1).getInteger();
            if (args.size() > 2) {
                fillchar = args.at(2).getString().get()[0];
            }
        }
        if (width > static_cast<std::int64_t>(res.size())) {
            std::size_t pad = (width - res.size()) / 2;
            res.insert(0, pad, fillchar);
            res.append(pad, fillchar);
        }
        return res;
    });

    constexpr auto ljust_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        std::int64_t width = 0;
        std::string fill = " ";
        if (options.isBlock()) {
            res = options.fn();
            width = args.at(0).getInteger();
            if (args.size() > 1) {
                fill = args.at(1).getString();
            }
        } else {
            res = args.at(0).getString();
            width = args.at(1).getInteger();
            if (args.size() > 2) {
                fill = args.at(2).getString();
            }
        }
        while (static_cast<std::int64_t>(res.size()) < width) {
            if (static_cast<std::int64_t>(res.size() + fill.size()) > width) {
                res.append(fill, 0, width - res.size());
            } else {
                res.append(fill);
            }
        }
        return res;
    };

    hbs.registerHelper("ljust", ljust_fn);
    hbs.registerHelper("pad_end", ljust_fn);

    static constexpr auto rjust_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        std::int64_t width = 0;
        std::string fill = " ";
        if (options.isBlock()) {
            res = options.fn();
            width = args.at(0).getInteger();
            if (args.size() > 1) {
                fill = args.at(1).getString();
            }
        } else {
            res = args.at(0).getString();
            width = args.at(1).getInteger();
            if (args.size() > 2) {
                fill = args.at(2).getString();
            }
        }
        while (static_cast<std::int64_t>(res.size()) < width) {
            if (static_cast<std::int64_t>(res.size() + fill.size()) > width) {
                res.insert(0, fill, 0, width - res.size());
            } else {
                res.insert(0, fill);
            }
        }
        return res;
    };

    hbs.registerHelper("rjust", rjust_fn);
    hbs.registerHelper("pad_start", rjust_fn);

    hbs.registerHelper("count", count_fn);

    hbs.registerHelper("ends_with", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string suffix;
        std::int64_t start = 0;
        std::int64_t end = 0;
        if (options.isBlock()) {
            str = options.fn();
            suffix = args.at(0).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 1) {
                start = args.at(1).getInteger();
                if (args.size() > 2) {
                    end = args.at(2).getInteger();
                }
            }
        } else {
            str = args.at(0).getString();
            suffix = args.at(1).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 2) {
                start = args.at(2).getInteger();
                if (args.size() > 3) {
                    end = args.at(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::string substr = str.substr(start, end - start);
        return substr.ends_with(suffix);
    });

    hbs.registerHelper("starts_with", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string prefix;
        std::int64_t start = 0;
        std::int64_t end = 0;
        if (options.isBlock()) {
            str = options.fn();
            prefix = args.at(0).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 1) {
                start = args.at(1).getInteger();
                if (args.size() > 2) {
                    end = args.at(2).getInteger();
                }
            }
        } else {
            str = args.at(0).getString();
            prefix = args.at(1).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 2) {
                start = args.at(2).getInteger();
                if (args.size() > 3) {
                    end = args.at(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::string substr = str.substr(start, end - start);
        return substr.starts_with(prefix);
    });

    hbs.registerHelper("expandtabs", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::int64_t tabsize = 8;
        if (options.isBlock()) {
            str = options.fn();
            if (!args.empty()) {
                tabsize = args.at(0).getInteger();
            }
        } else {
            str = args.at(0).getString();
            if (args.size() > 1) {
                tabsize = args.at(1).getInteger();
            }
        }
        std::string res;
        res.reserve(str.size());
        for (char c : str) {
            if (c == '\t') {
                res.append(tabsize, ' ');
            } else {
                res.push_back(c);
            }
        }
        return res;
    });

    static constexpr auto find_index_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string sub;
        std::int64_t start = 0;
        std::int64_t end = 0;
        if (options.isBlock()) {
            str = options.fn();
            sub = args.at(0).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 1) {
                start = args.at(1).getInteger();
                if (args.size() > 2) {
                    end = args.at(2).getInteger();
                }
            }
        } else {
            str = args.at(0).getString();
            sub = args.at(1).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 2) {
                start = args.at(2).getInteger();
                if (args.size() > 3) {
                    end = args.at(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::size_t pos = str.find(sub, start);
        if (pos == std::string::npos || static_cast<std::int64_t>(pos) >= end) {
            return static_cast<std::int64_t>(-1);
        }
        return static_cast<std::int64_t>(pos);
    };

    hbs.registerHelper("find", find_index_fn);
    hbs.registerHelper("index_of", find_index_fn);

    hbs.registerHelper("includes", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        return find_index_fn(args, options) != -1;
    });

    auto rfind_index_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string sub;
        std::int64_t start = 0;
        std::int64_t end = 0;
        if (options.isBlock()) {
            str = options.fn();
            sub = args.at(0).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 1) {
                start = args.at(1).getInteger();
                if (args.size() > 2) {
                    end = args.at(2).getInteger();
                }
            }
        } else {
            str = args.at(0).getString();
            sub = args.at(1).getString();
            end = static_cast<std::int64_t>(str.size());
            if (args.size() > 2) {
                start = args.at(2).getInteger();
                if (args.size() > 3) {
                    end = args.at(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::size_t pos = str.rfind(sub, start);
        if (pos == std::string::npos || static_cast<std::int64_t>(pos) >= end) {
            return static_cast<std::int64_t>(-1);
        }
        return static_cast<std::int64_t>(pos);
    };

    hbs.registerHelper("rfind", rfind_index_fn);
    hbs.registerHelper("rindex_of", rfind_index_fn);
    hbs.registerHelper("last_index_of", rfind_index_fn);

    hbs.registerHelper("at", at_fn);
    hbs.registerHelper("char_at", at_fn);

    hbs.registerHelper("is_alnum", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return (c >= '0' && c <= '9') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z');
        });
    });

    hbs.registerHelper("is_alpha", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z');
        });
    });

    hbs.registerHelper("is_ascii", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return std::ranges::all_of(res, [](char c) {
            auto uc = static_cast<unsigned char>(c);
            return uc <= 127;
        });
    });

    auto is_digits_fn = [](dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c >= '0' && c <= '9';
        });
    };

    hbs.registerHelper("is_decimal", is_digits_fn);
    hbs.registerHelper("is_digit", is_digits_fn);

    hbs.registerHelper("is_lower",
       [](dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c >= 'a' && c <= 'z';
        });
    });

    hbs.registerHelper("is_upper",
       [](dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c >= 'A' && c <= 'Z';
        });
    });

    hbs.registerHelper("is_printable",
       [](dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c >= 32 && c <= 126;
        });
    });

    hbs.registerHelper("is_space",
       [](dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c == ' ' || (c >= 9 && c <= 13);
        });
    });

    hbs.registerHelper("is_title",
       [](dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        bool prev_is_cased = false;
        bool res_is_title = false;
        for (char c : res) {
            if (c >= 'A' && c <= 'Z') {
                if (prev_is_cased) {
                    return false;
                }
                prev_is_cased = true;
                res_is_title = true;
            } else if (c >= 'a' && c <= 'z') {
                if (!prev_is_cased) {
                    return false;
                }
                prev_is_cased = true;
            } else {
                prev_is_cased = false;
            }
        }
        return res_is_title;
    });

    static constexpr auto to_upper_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        std::ranges::transform(res, res.begin(), toupper);
        return res;
    };

    hbs.registerHelper("upper", to_upper_fn);
    hbs.registerHelper("to_upper", to_upper_fn);

    static constexpr auto to_lower_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        std::ranges::transform(res, res.begin(), tolower);
        return res;
    };

    hbs.registerHelper("lower", to_lower_fn);
    hbs.registerHelper("to_lower", to_lower_fn);

    hbs.registerHelper("swap_case", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        if (res.empty()) {
            return res;
        }
        std::ranges::transform(res, res.begin(), [](char c) -> char {
            if (c >= 'A' && c <= 'Z') {
                return tolower(c);
            } else if (c >= 'a' && c <= 'z') {
                return toupper(c);
            } else {
                return c;
            }
        });
        return res;
    });

    static constexpr auto join_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        dom::Array arr;
        if (options.isBlock()) {
            str = options.fn();
            arr = args.at(0).getArray();
        } else {
            str = args.at(0).getString();
            arr = args.at(1).getArray();
        }
        std::string res;
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!res.empty()) {
                res += str;
            }
            auto const& item = arr.at(i);
            res += item.getString();
        }
        return res;
    };

    hbs.registerHelper("join", join_fn);
    hbs.registerHelper("implode", join_fn);

    hbs.registerHelper("concat", concat_fn);

    static constexpr auto strip_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string chars = " \t\r\n";
        if (options.isBlock()) {
            str = options.fn();
            if (!args.empty())
                chars = args.at(0).getString();
        } else {
            str = args.at(0).getString();
            if (args.size() > 1)
                chars = args.at(1).getString();
        }
        std::size_t pos = str.find_first_not_of(chars);
        if (pos == std::string::npos) {
            return std::string();
        }
        std::size_t endpos = str.find_last_not_of(chars);
        return str.substr(pos, endpos - pos + 1);
    };

    hbs.registerHelper("strip", strip_fn);
    hbs.registerHelper("trim", strip_fn);

    static constexpr auto lstrip_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string chars = " \t\r\n";
        if (options.isBlock()) {
            str = options.fn();
            if (!args.empty())
                chars = args.at(0).getString();
        } else {
            str = args.at(0).getString();
            if (args.size() > 1)
                chars = args.at(1).getString();
        }
        std::size_t pos = str.find_first_not_of(chars);
        if (pos == std::string::npos) {
            return std::string();
        }
        return str.substr(pos);
    };

    hbs.registerHelper("lstrip", lstrip_fn);
    hbs.registerHelper("trim_start", lstrip_fn);

    static constexpr auto rstrip_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string chars = " \t\r\n";
        if (options.isBlock()) {
            str = options.fn();
            if (!args.empty())
                chars = args.at(0).getString();
        } else {
            str = args.at(0).getString();
            if (args.size() > 1)
                chars = args.at(1).getString();
        }
        std::size_t pos = str.find_last_not_of(chars);
        if (pos == std::string::npos) {
            return std::string();
        }
        return str.substr(0, pos + 1);
    };

    hbs.registerHelper("rstrip", rstrip_fn);
    hbs.registerHelper("trim_end", rstrip_fn);

    hbs.registerHelper("partition", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string sep;
        if (options.isBlock()) {
            str = options.fn();
            sep = args.at(0).getString();
        } else {
            str = args.at(0).getString();
            sep = args.at(1).getString();
        }
        dom::Array res;
        std::size_t pos = str.find(sep);
        if (pos == std::string::npos) {
            res.emplace_back(str);
            res.emplace_back(std::string());
            res.emplace_back(std::string());
        } else {
            res.emplace_back(str.substr(0, pos));
            res.emplace_back(sep);
            res.emplace_back(str.substr(pos + sep.size()));
        }
        return res;
    });

    hbs.registerHelper("rpartition", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string sep;
        if (options.isBlock()) {
            str = options.fn();
            sep = args.at(0).getString();
        } else {
            str = args.at(0).getString();
            sep = args.at(1).getString();
        }
        dom::Array res;
        std::size_t pos = str.rfind(sep);
        if (pos == std::string::npos) {
            res.emplace_back(str);
            res.emplace_back(std::string());
            res.emplace_back(std::string());
        } else {
            res.emplace_back(str.substr(0, pos));
            res.emplace_back(sep);
            res.emplace_back(str.substr(pos + sep.size()));
        }
        return res;
    });

    hbs.registerHelper("remove_prefix", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string prefix;
        if (options.isBlock()) {
            str = options.fn();
            prefix = args.at(0).getString();
        } else {
            str = args.at(0).getString();
            prefix = args.at(1).getString();
        }
        if (str.size() < prefix.size()) {
            return str;
        }
        if (str.substr(0, prefix.size()) != prefix) {
            return str;
        }
        return str.substr(prefix.size());
    });

    hbs.registerHelper("remove_suffix", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string suffix;
        if (options.isBlock()) {
            str = options.fn();
            suffix = args.at(0).getString();
        } else {
            str = args.at(0).getString();
            suffix = args.at(1).getString();
        }
        if (str.size() < suffix.size()) {
            return str;
        }
        if (str.substr(str.size() - suffix.size()) != suffix) {
            return str;
        }
        return str.substr(0, str.size() - suffix.size());
    });

    hbs.registerHelper("replace", replace_fn);

    static constexpr auto split_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string sep = " ";
        std::int64_t maxsplit = -1;
        if (options.isBlock()) {
            str = options.fn();
            if (!args.empty())
                sep = args.at(0).getString();
            if (args.size() > 1)
                maxsplit = args.at(1).getInteger();
        } else {
            str = args.at(0).getString();
            if (args.size() > 1)
                sep = args.at(1).getString();
            if (args.size() > 2)
                maxsplit = args.at(2).getInteger();
        }
        dom::Array res;
        std::size_t pos = 0;
        std::size_t sep_len = sep.size();
        while (maxsplit != 0) {
            std::size_t next = str.find(sep, pos);
            if (next == std::string::npos) {
                res.emplace_back(str.substr(pos));
                break;
            }
            res.emplace_back(str.substr(pos, next - pos));
            pos = next + sep_len;
            if (maxsplit > 0) {
                --maxsplit;
            }
        }
        return res;
    };

    hbs.registerHelper("split", split_fn);
    hbs.registerHelper("explode", split_fn);

    hbs.registerHelper("rsplit", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        std::string sep = " ";
        std::int64_t maxsplit = -1;
        if (options.isBlock()) {
            str = options.fn();
            if (!args.empty())
                sep = args.at(0).getString();
            if (args.size() > 1)
                maxsplit = args.at(1).getInteger();
        } else {
            str = args.at(0).getString();
            if (args.size() > 1)
                sep = args.at(1).getString();
            if (args.size() > 2)
                maxsplit = args.at(2).getInteger();
        }
        dom::Array res;
        std::size_t pos = str.size();
        std::size_t sep_len = sep.size();
        while (maxsplit != 0) {
            std::size_t next = str.rfind(sep, pos);
            if (next == std::string::npos) {
                res.emplace_back(str.substr(0, pos));
                break;
            }
            res.emplace_back(str.substr(next + sep_len, pos - next - sep_len));
            if (next == 0) {
                break;
            }
            pos = next - 1;
            if (maxsplit > 0) {
                --maxsplit;
            }
        }
        return res;
    });

    hbs.registerHelper("split_lines", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string str;
        bool keepends = false;
        if (options.isBlock()) {
            str = options.fn();
            if (!args.empty())
                keepends = args.at(0).getBool();
        } else {
            str = args.at(0).getString();
            if (args.size() > 1)
                keepends = args.at(1).getBool();
        }
        dom::Array res;
        std::size_t pos = 0;
        std::size_t len = str.size();
        while (pos < len) {
            std::size_t next = str.find_first_of("\r\n", pos);
            if (next == std::string::npos) {
                res.emplace_back(str.substr(pos));
                break;
            }
            if (keepends) {
                res.emplace_back(str.substr(pos, next - pos + 1));
            } else {
                res.emplace_back(str.substr(pos, next - pos));
            }
            pos = next + 1;
        }
        return res;
    });

    hbs.registerHelper("zfill", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        std::int64_t width = 0;
        if (options.isBlock()) {
            res = options.fn();
            width = args.at(0).getInteger();
        } else {
            res = args.at(0).getString();
            width = args.at(1).getInteger();
        }
        if (width <= static_cast<std::int64_t>(res.size())) {
            return res;
        }
        std::string prefix;
        if (res[0] == '+' || res[0] == '-') {
            prefix = res[0];
            res = res.substr(1);
            if (width != static_cast<std::int64_t>(res.size()))
                --width;
        }
        std::string padding(width - res.size(), '0');
        return prefix + padding + res;
    });

    hbs.registerHelper("repeat", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        std::int64_t count = 0;
        if (options.isBlock()) {
            res = options.fn();
            count = args.at(0).getInteger();
        } else {
            res = args.at(0).getString();
            count = args.at(1).getInteger();
        }
        if (count <= 0) {
            return std::string();
        }
        std::string tmp;
        while (count > 0) {
            tmp += res;
            --count;
        }
        return tmp;
    });

    hbs.registerHelper("escape", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        return escapeExpression(res);
    });

    static constexpr auto slice_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        std::int64_t start = 0;
        std::int64_t stop = 0;
        if (options.isBlock()) {
            res = options.fn();
            stop = static_cast<std::int64_t>(res.size());
            start = args.at(0).getInteger();
            if (args.size() > 1)
                stop = args.at(1).getInteger();
        } else {
            res = args.at(0).getString();
            stop = static_cast<std::int64_t>(res.size());
            start = args.at(1).getInteger();
            if (args.size() > 2)
                stop = args.at(2).getInteger();
        }
        if (res.empty()) {
            return std::string();
        }
        start = normalize_index(start, static_cast<std::int64_t>(res.size()));
        stop = normalize_index(stop, static_cast<std::int64_t>(res.size()));
        if (start >= stop) {
            return std::string();
        }
        return res.substr(start, stop - start);
    };

    hbs.registerHelper("slice", slice_fn);
    hbs.registerHelper("substr", slice_fn);

    hbs.registerHelper("safe_anchor_id", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        std::ranges::transform(res, res.begin(), [](char c) {
            if (c == ' ' || c == '_')
                return '-';
           return tolower(c);
        });
        // remove any ":" from the string
        auto it = std::remove(res.begin(), res.end(), ':');
        res.erase(it, res.end());
        return res;
    });

    hbs.registerHelper("strip_namespace", [](
        dom::Array const& args, HandlebarsCallback const& options) {
        std::string res;
        if (options.isBlock()) {
            res = options.fn();
        } else {
            res = args.at(0).getString();
        }
        auto inside = 0;
        size_t count = 0;
        size_t offset = std::string::npos;
        for (auto const& c: res) {
            switch (c) {
            case '(':
            case '[':
            case '<':
            {
                inside++;
                break;
            }
            case ')':
            case ']':
            case '>':
            {
                inside--;
                break;
            }
            case ':':
            {
                if (inside == 0) {
                    offset = count + 1;
                }
                break;
            }
            default:
            {
                break;
            }
            }
            count++;
        }
        if (offset != std::string::npos) {
            return res.substr(offset);
        } else {
            return res;
        }
    });
}

void
registerContainerHelpers(Handlebars& hbs)
{
    static constexpr auto size_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        auto const& val = args.at(0);
        switch (val.kind()) {
        case dom::Kind::Array:
            return static_cast<std::int64_t>(val.getArray().size());
        case dom::Kind::Object:
            return static_cast<std::int64_t>(val.getObject().size());
        case dom::Kind::String:
            return static_cast<std::int64_t>(val.getString().size());
        case dom::Kind::Null:
            return static_cast<std::int64_t>(0);
        default:
            return static_cast<std::int64_t>(1);
        }
    };

    hbs.registerHelper("size", size_fn);
    hbs.registerHelper("len", size_fn);

    static constexpr auto keys_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        auto container = args.at(0);
        auto const& obj = container.getObject();
        dom::Array res;
        for (auto const& [key, _]: obj)
        {
            res.emplace_back(key);
        }
        return res;
    };

    hbs.registerHelper("keys", keys_fn);
    hbs.registerHelper("list", keys_fn);
    hbs.registerHelper("iter", keys_fn);

    static constexpr auto values_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        auto container = args.at(0);
        if (!container.isObject()) {
            return container;
        }
        auto const& obj = container.getObject();
        dom::Array res;
        for (auto const& [_, value]: obj)
        {
            res.emplace_back(value);
        }
        return res;
    };

    hbs.registerHelper("values", values_fn);

    static constexpr auto del_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        dom::Value range = args.at(0);
        dom::Value item = args.at(1);
        if (range.isArray()) {
            auto const& arr = range.getArray();
            auto const& val = item;
            dom::Array res;
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(arr.size()); i++)
            {
                if (!isSame(arr[i], val))
                    res.emplace_back(arr.at(i));
            }
            return res;
        } else if (range.isObject()) {
            auto const& obj = range.getObject();
            auto const& key = item.getString();
            dom::Object res;
            for (auto const& [k, v]: obj)
            {
                if (k != key)
                res.set(k, v);
            }
            return res;
        } else {
            return range;
        }
    };

    hbs.registerHelper("del", del_fn);
    hbs.registerHelper("delete", del_fn);

    static constexpr auto has_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        dom::Value ctx = args.at(0);
        dom::Value prop = args.at(1);
        if (ctx.isObject()) {
            dom::Value objV = ctx;
            auto const& obj = objV.getObject();
            auto const& key = prop.getString();
            return obj.exists(key);
        } else if (ctx.isArray()){
            dom::Value arrV = ctx;
            auto const& arr = arrV.getArray();
            auto value = prop;
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (isSame(arr[i], value))
                    return true;
            }
        }
        return false;
    };

    hbs.registerHelper("has", has_fn);
    hbs.registerHelper("exist", has_fn);
    hbs.registerHelper("contains", has_fn);

    static constexpr auto has_any_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        auto container = args.at(0);
        auto item = args.at(1);
        if (container.isObject()) {
            dom::Value const& objV = container;
            auto const& obj = objV.getObject();
            dom::Value const& keysV = item;
            auto const& keys = keysV.getArray();
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(keys.size()); ++i) {
                dom::Value k = keys[i];
                if (obj.exists(k.getString()))
                    return true;
            }
            return false;
        } else if (container.isArray()){
            auto const& arr = container.getArray();
            auto const& values = item.getArray();
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(values.size()); ++i) {
                for (std::size_t j = 0; j < arr.size(); ++j) {
                    dom::Value a = arr[j];
                    dom::Value b = values[i];
                    if (isSame(a, b)) {
                        return true;
                    }
                }
            }
            return false;
        } else {
            return false;
        }
    };

    hbs.registerHelper("has_any", has_any_fn);
    hbs.registerHelper("exist_any", has_any_fn);
    hbs.registerHelper("contains_any", has_any_fn);

    static constexpr auto get_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) {
        auto fieldV = args.at(1);
        dom::Value default_value = nullptr;
        if (args.size() > 2)
            default_value = args.at(2);
        dom::Value argsV = args.at(0);
        if (argsV.isArray()) {
            auto const& arr = argsV.getArray();
            auto index = fieldV.getInteger();
            if (index < 0)
                index = normalize_index(index, static_cast<std::int64_t>(arr.size()));
            if (index >= static_cast<std::int64_t>(arr.size()))
                return default_value;
            return arr.at(index);
        } else if (argsV.isObject()) {
            auto const& obj = argsV.getObject();
            auto const& key = fieldV.getString();
            if (obj.exists(key))
                return obj.find(key);
            return default_value;
        } else {
            return default_value;
        }
    };

    hbs.registerHelper("get", get_fn);
    hbs.registerHelper("get_or", get_fn);

    static constexpr auto items_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        dom::Value itemsV = args.at(0);
        if (itemsV.isObject()) {
            auto const& obj = itemsV.getObject();
            dom::Array res;
            for (auto const& [key, value]: obj)
            {
                dom::Array item;
                item.emplace_back(key);
                item.emplace_back(value);
                res.emplace_back(item);
            }
            return res;
        } else {
            return itemsV;
        }
    };

    hbs.registerHelper("items", items_fn);
    hbs.registerHelper("entries", items_fn);

    static constexpr auto first_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        auto range = args.at(0);
        if (range.isArray()) {
            auto const& arr = range.getArray();
            if (arr.empty())
                return nullptr;
            return arr.at(0);
        } else if (range.isObject()) {
            auto const& obj = range.getObject();
            if (obj.empty())
                return nullptr;
            return obj.get(0).value;
        } else {
            return range;
        }
    };

    hbs.registerHelper("first", first_fn);
    hbs.registerHelper("head", first_fn);
    hbs.registerHelper("front", first_fn);

    static constexpr auto last_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        auto range = args.at(0);
        if (range.isArray()) {
            auto const& arr = range.getArray();
            if (arr.empty())
                return nullptr;
            return arr.at(arr.size() - 1);
        } else if (range.isObject()) {
            auto const& obj = range.getObject();
            if (obj.empty())
                return nullptr;
            return obj.get(obj.size() - 1).value;
        } else {
            return range;
        }
    };

    hbs.registerHelper("last", last_fn);
    hbs.registerHelper("tail", last_fn);
    hbs.registerHelper("back", last_fn);

    static constexpr auto reverse_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        dom::Value container = args.at(0);
        if (container.isArray()) {
            auto const& arr = container.getArray();
            dom::Array res;
            for (std::size_t i = arr.size(); i > 0; --i) {
                res.emplace_back(arr.at(i - 1));
            }
            return res;
        } else if (container.isObject()) {
            auto const& obj = container.getObject();
            dom::Array res;
            for (auto const& [key, value]: obj)
            {
                dom::Array item;
                item.emplace_back(key);
                item.emplace_back(value);
                res.emplace_back(item);
            }
            dom::Array reversed;
            for (std::size_t i = res.size(); i > 0; --i) {
                reversed.emplace_back(res.at(i - 1));
            }
            return reversed;
        } else {
            return container;
        }
    };

    hbs.registerHelper("reverse", reverse_fn);
    hbs.registerHelper("reversed", reverse_fn);

    static constexpr auto update_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        auto container = args.at(0);
        auto otherContainer = args.at(1);
        if (container.isObject()) {
            auto const& obj = container.getObject();
            auto const& other = otherContainer.getObject();
            dom::Object res = createFrame(obj);
            for (auto const& [k, v]: other) {
                res.set(k, v);
            }
            return res;
        } else if (container.isArray()) {
            auto const& arr = container.getArray();
            auto const& other = otherContainer.getArray();
            dom::Array res;
            for (std::size_t i = 0; i < arr.size(); ++i) {
                res.emplace_back(arr.at(i));
            }
            for (std::size_t i = 0; i < other.size(); ++i) {
                bool arr_contains = false;
                for (std::size_t j = 0; j < res.size(); ++j) {
                    if (isSame(res.at(j), other.at(i))) {
                        arr_contains = true;
                        break;
                    }
                }
                if (!arr_contains) {
                    res.emplace_back(other.at(i));
                }
            }
            return res;
        } else {
            return container;
        }
    };

    hbs.registerHelper("update", update_fn);
    hbs.registerHelper("merge", update_fn);

    static constexpr auto sort_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        auto container = args.at(0);
        if (container.isArray()) {
            auto const& arr = container.getArray();
            std::vector<dom::Value> res;
            for (std::size_t i = 0; i < arr.size(); ++i) {
                res.emplace_back(arr.at(i));
            }
            std::sort(res.begin(), res.end(), [](auto const& a, auto const& b) {
                return isLt(a, b);
            });
            dom::Array res2;
            for (const auto & re : res) {
                res2.emplace_back(re);
            }
            return res2;
        } else {
            return container;
        }
    };

    hbs.registerHelper("sort", sort_fn);

    static constexpr auto sort_by_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        // Given an array of objects, sort these objects by their value at
        // a given key
        auto container = args.at(0);
        if (container.isArray()) {
            auto const& arr = container.getArray();
            dom::Value keyV = args.at(1);
            auto const& key = keyV.getString();
            std::vector<dom::Value> res;
            for (std::size_t i = 0; i < arr.size(); ++i) {
                res.emplace_back(arr.at(i));
            }
            std::sort(res.begin(), res.end(), [&key](dom::Value const& a, dom::Value const& b) {
                if (!a.isObject() || !b.isObject()) {
                    if (a.isObject())
                        return true;
                    if (b.isObject())
                        return false;
                    return isLt(a, b);
                }
                const bool ak = a.getObject().exists(key);
                const bool bk = b.getObject().exists(key);
                if (!ak) {
                    return bk;
                }
                if (!bk) {
                    return false;
                }
                return isLt(a.getObject().find(key), b.getObject().find(key));
            });
            dom::Array res2;
            for (const auto & re : res) {
                res2.emplace_back(re);
            }
            return res2;
        } else {
            return container;
        }
    };

    hbs.registerHelper("sort_by", sort_by_fn);

    hbs.registerHelper("at", at_fn);

    static constexpr auto fill_fn = [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        auto container = args.at(0);
        if (container.isArray()) {
            auto const& arr = container.getArray();
            dom::Value fill_value = args.at(1);
            std::int64_t start = 0;
            if (args.size() > 2)
                start = args.at(2).getInteger();
            auto stop = static_cast<std::int64_t>(arr.size());
            if (args.size() > 3)
                stop = args.at(3).getInteger();
            start = normalize_index(start, static_cast<std::int64_t>(arr.size()));
            stop = normalize_index(stop, static_cast<std::int64_t>(arr.size()));
            dom::Array res;
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(arr.size()); ++i) {
                if (i >= start && i < stop)
                    res.emplace_back(fill_value);
                else
                    res.emplace_back(arr.at(i));
            }
            return res;
        } else {
            return container;
        }
    };

    hbs.registerHelper("fill", fill_fn);

    hbs.registerHelper("count", count_fn);
    hbs.registerHelper("replace", replace_fn);

    hbs.registerHelper("chunk", [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        dom::Value range = args.at(0);
        std::int64_t size = args.at(1).getInteger();
        if (range.isArray()) {
            auto const& arr = range.getArray();
            dom::Array res;
            std::int64_t i = 0;
            while (i < static_cast<std::int64_t>(arr.size())) {
                dom::Array chunk;
                for (std::int64_t j = 0; j < size && i < static_cast<std::int64_t>(arr.size()); ++j) {
                    chunk.emplace_back(arr.at(i));
                    ++i;
                }
                res.emplace_back(chunk);
            }
            return res;
        } else if (range.isString()) {
            auto const& str = range.getString();
            dom::Array res;
            std::int64_t i = 0;
            while (i < static_cast<std::int64_t>(str.size())) {
                std::string chunk;
                for (std::int64_t j = 0;
                     j < size && i < static_cast<std::int64_t>(str.size());
                     ++j)
                {
                    chunk += str.get()[i];
                    ++i;
                }
                res.emplace_back(chunk);
            }
            return res;
        } else if (range.isObject()) {
            auto const& obj = range.getObject();
            dom::Array res;
            std::int64_t i = 0;
            while (i < static_cast<std::int64_t>(obj.size())) {
                dom::Object chunk;
                for (std::int64_t j = 0; j < size && i < static_cast<std::int64_t>(obj.size()); ++j) {
                    auto const& [k, v] = obj.at(i);
                    chunk.set(k, v);
                    ++i;
                }
                res.emplace_back(chunk);
            }
            return res;
        } else {
            return range;
        }
    });

    hbs.registerHelper("group_by", [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        // given an array of objects, group these objects by a key in each them
        // the result is
        dom::Value range = args.at(0);
        if (!range.isArray()) {
            return range;
        }
        dom::Array const& array = range.getArray();
        std::string key(args.at(1).getString());
        std::vector<std::uint8_t> copied(array.size(), 0);
        dom::Object res;
        for (std::size_t i = 0; i < array.size(); ++i) {
            if (copied[i] || !array[i].getObject().exists(key)) {
                // object already copied or doesn't have the key
                copied[i] = 0x01;
                continue;
            }
            copied[i] = 0x01;
            // Create a group name for this key value
            std::string group_name(array[i].getObject().find(key).getString());
            dom::Array group;
            group.emplace_back(array[i]);
            for (std::size_t j = i; j < array.size(); ++j) {
                if (copied[j]) {
                    continue;
                }
                if (array[j].getObject().find(key).getString() ==
                    array[i].getObject().find(key).getString()) {
                    group.emplace_back(array[j]);
                    copied[j] = 0x01;
                }
            }
            res.set(group_name, group);
        }
        return res;
    });

    hbs.registerHelper("pluck", [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        // given an array of objects, take the value of a key from each object
        dom::Value rangeV = args.at(0);
        if (!rangeV.isArray()) {
            return rangeV;
        }
        dom::Array const& range = rangeV.getArray();
        std::string key(args.at(1).getString());
        dom::Array res;
        for (std::int64_t i = 0; i < static_cast<std::int64_t>(range.size()); ++i) {
            if (range[i].isObject() && range[i].getObject().exists(key)) {
                res.emplace_back(range[i].getObject().find(key));
            }
        }
        return res;
    });

    hbs.registerHelper("unique", [](
        dom::Array const& args, HandlebarsCallback const& options) -> dom::Value {
        dom::Value const& rangeV = args.at(0);
        if (!rangeV.isArray()) {
            return rangeV;
        }
        dom::Array const& range = rangeV.getArray();
        std::vector<dom::Value> res;
        for (std::int64_t i = 0; i < static_cast<std::int64_t>(range.size()); ++i) {
            res.push_back(range[i]);
        }
        std::sort(res.begin(), res.end(), [](dom::Value const& a, dom::Value const& b) {
            return isLt(a, b);
        });
        auto last = std::unique(res.begin(), res.end(), [](dom::Value const& a, dom::Value const& b) {
            return isSame(a, b);
        });
        res.erase(last, res.end());
        dom::Array res2;
        for (auto const& v : res) {
            res2.emplace_back(v);
        }
        return res2;
    });

    hbs.registerHelper("concat", concat_fn);
}

} // helpers

} // mrdox
} // clang

