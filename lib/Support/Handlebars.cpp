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

/////////////////////////////////////////////////////////////////
// Utility functions
/////////////////////////////////////////////////////////////////

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
            return !arg.getArray().empty();
        case dom::Kind::Object:
            return !arg.getObject().empty();
        case dom::Kind::Null:
        default:
            return false;
    }
}

bool
isEmpty(dom::Value const& arg)
{
    if (arg.isArray())
        return arg.getArray().empty();
    if (arg.isObject())
        return arg.getObject().empty();
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
    if (opt.noHTMLEscape)
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
    } else if (value.isNull()) {
        out << "null";
    }
}

std::string_view
trim_spaces(std::string_view expression)
{
    auto pos = expression.find_first_not_of(" \t\r\n");
    if (pos == std::string_view::npos)
        return "";
    expression.remove_prefix(pos);
    pos = expression.find_last_not_of(" \t\r\n");
    if (pos == std::string_view::npos)
        return "";
    expression.remove_suffix(expression.size() - pos - 1);
    return expression;
}

std::string_view
trim_lspaces(std::string_view expression)
{
    auto pos = expression.find_first_not_of(" \t\r\n");
    if (pos == std::string_view::npos)
        return "";
    expression.remove_prefix(pos);
    return expression;
}

std::string_view
trim_rspaces(std::string_view expression)
{
    auto pos = expression.find_last_not_of(" \t\r\n");
    if (pos == std::string_view::npos)
        return "";
    expression.remove_suffix(expression.size() - pos - 1);
    return expression;
}

/////////////////////////////////////////////////////////////////
// Helper Callback
/////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////
// Engine
/////////////////////////////////////////////////////////////////

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
            opt.noHTMLEscape = true;
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
    HandlebarsOptions options) const
{
    std::string out;
    OutputRef os(out);
    render_to(
        os, templateText, context, options);
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
    if (templateText.substr(pos).starts_with("{{!--")) {
        closeTagToken = "--}}";
    } else if (templateText.substr(pos).starts_with("{{{{")) {
        closeTagToken = "}}}}";
    } else if (templateText.substr(pos).starts_with("{{{")) {
        closeTagToken = "}}}";
    }
    auto end = templateText.find(closeTagToken, pos);
    if (end == std::string_view::npos)
        return false;

    // Found tag
    tag = templateText.substr(pos, end - pos + closeTagToken.size());

    // Check if tag is escaped verbatim
    if (pos != 0 && templateText[pos - 1] == '\\') {
        tag = {tag.begin() - 1, tag.end()};
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
    expr = tagContent.substr(0, pos);
    return pos != 0;
}


// Parse a tag into helper, expression and content
Handlebars::Tag
parseTag(std::string_view tagStr)
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

    // Force no HTML escape
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

    // Just get the content of expression is escaped
    if (t.escaped) {
        t.content = tagStr;
        t.arguments = tagStr;
        return t;
    }

    // Remove whitespaces once again to support tags with extra whitespaces.
    // This makes invalid tags like "{{ #if condition }}" work instead of failing.
    tagStr = trim_spaces(tagStr);

    // Identify whitespace removal
    if (tagStr.starts_with('~')) {
        t.removeLWhitespace = true;
        tagStr.remove_prefix(1);
        tagStr = trim_spaces(tagStr);
    }
    if (tagStr.ends_with('~')) {
        t.removeRWhitespace = true;
        tagStr.remove_suffix(1);
        tagStr = trim_spaces(tagStr);
    }

    // Empty tag
    if (tagStr.empty())
    {
        t.type = ' ';
        t.content = tagStr.substr(0);
        t.helper = tagStr.substr(0);
        t.content = tagStr.substr(0);
        t.arguments = tagStr.substr(0);
        return t;
    }

    // Find tag type
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

    // Get block parameters
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

    // Find helper and expression
    std::string_view expr;
    if (findExpr(expr, tagStr)) {
        t.helper = expr;
        tagStr.remove_prefix(expr.data() + expr.size() - tagStr.data());
        t.arguments = trim_spaces(tagStr);
    } else {
        t.helper = tagStr;
        t.arguments = {};
    }
    return t;
}

void
Handlebars::
render_to(
    OutputRef& out,
    std::string_view templateText,
    dom::Value const & context,
    HandlebarsOptions options) const
{
    partials_map extra_partials;
    dom::Object private_data;
    private_data.set("root", context);
    private_data.set("level", "warn");
    dom::Object blockValues;
    render_to(
        out, templateText, context, options,
        extra_partials, private_data, blockValues);
}

void
Handlebars::
render_to(
    OutputRef& out,
    std::string_view templateText,
    dom::Value const &data,
    HandlebarsOptions opt,
    partials_map& extra_partials,
    dom::Object const& private_data,
    dom::Object const& blockValues) const
{
    while (!templateText.empty()) {
        std::string_view tagStr;
        if (!findTag(tagStr, templateText))
        {
            out << templateText;
            break;
        }
        std::size_t tagStartPos = tagStr.data() - templateText.data();
        Tag tag = parseTag(tagStr);
        std::size_t templateEndPos = tagStartPos;
        if (tag.removeLWhitespace) {
            std::string_view beforeTag = templateText.substr(0, tagStartPos);
            auto pos = beforeTag.find_last_not_of(" \t\r\n");
            if (pos != std::string_view::npos) {
                templateEndPos = pos + 1;
            } else {
                templateEndPos = 0;
            }
        }
        out << templateText.substr(0, templateEndPos);
        templateText.remove_prefix(tagStartPos + tagStr.size());
        if (!tag.escaped) {
            renderTag(
                tag, out, templateText, data, opt,
                extra_partials, private_data, blockValues);
        } else {
            out << tag.content;
        }
    }
}

std::pair<dom::Value, bool>
lookupProperty(
    dom::Object const &data,
    std::string_view path)
{
    auto nextSegment = [](std::string_view path)
        -> std::pair<std::string_view, std::string_view> {
        // Skip dot segments
        while (path.starts_with("./") || path.starts_with("[.]/") || path.starts_with("[.]."))
        {
            path.remove_prefix(path.front() == '.' ? 2 : 4);
        }
        // All that's left is a dot segment, so there are no more valid paths
        if (path == "." || path == "[.]")
            return {{}, {}};
        // If path starts with "[" the logic changes for literal segments
        if (path.starts_with('['))
        {
            auto pos = path.find_first_of(']');
            if (pos == std::string_view::npos)
            {
                // '[' segment was never closed
                return {{}, {}};
            }
            std::string_view seg = path.substr(1, pos - 1);
            path = path.substr(pos + 1);
            if (path.empty())
            {
                // rest of the path is empty, so this is the last segment
                return {seg, path};
            }
            if (path.front() != '.' && path.front() != '/')
            {
                // segment has no valid continuation, so it's invalid
                return {{}, path};
            }
            return { seg, path.substr(1) };
        }
        // If path starts with dotdot segment, the delimiter needs to be a slash
        // Otherwise, it can be either a slash or a dot
        std::string_view delimiters = path.starts_with("..") ? "/" : "./";
        auto pos = path.find_first_of(delimiters);
        // If no delimiter, the whole path is the segment
        if (pos == std::string_view::npos)
            return {path, {}};
        // Otherwise, the segment is up to the delimiter
        return {path.substr(0, pos), path.substr(pos + 1)};
    };

    // Get first value from Object
    std::string_view segment;
    std::tie(segment, path) = nextSegment(path);
    if (!data.exists(segment))
        return {nullptr, false};
    dom::Value cur = data.find(segment);

    // Recursively get more values from current value
    std::tie(segment, path) = nextSegment(path);
    while (!segment.empty())
    {
        // If current value is an Object, get the next value from it
        if (cur.isObject())
        {
            auto obj = cur.getObject();
            if (obj.exists(segment))
                cur = obj.find(segment);
            else
                return {nullptr, false};
        }
        // If current value is an Array, get the next value the stripped index
        else if (cur.isArray())
        {
            size_t index;
            std::from_chars_result res = std::from_chars(
                segment.data(),
                segment.data() + segment.size(),
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
        std::tie(segment, path) = nextSegment(path);
    }
    return {cur, true};
}

std::pair<dom::Value, bool>
lookupProperty(
    dom::Value const &data,
    std::string_view path) {
    if (path == "." || path == "this" || path == "[.]" || path == "[this]" || path.empty())
        return {data, true};
    if (data.kind() != dom::Kind::Object) {
        return {nullptr, false};
    }
    return lookupProperty(data.getObject(), path);
}

std::pair<dom::Value, bool>
lookupProperty(
    dom::Value const& data,
    dom::Value const& path)
{
    if (path.isString())
        return lookupProperty(data, path.getString());
    if (path.isInteger()) {
        if (data.isArray()) {
            auto& arr = data.getArray();
            if (path.getInteger() >= arr.size())
                return {nullptr, false};
            return {arr.at(path.getInteger()), true};
        }
        return lookupProperty(data, std::to_string(path.getInteger()));
    }
    return {nullptr, false};
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
        s += " ]";
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
    if (expression.empty())
        return false;
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

std::string unescapeString(std::string_view str) {
    std::string unescapedString;
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
        return false;
    }
    contextPath = contextPath.substr(0, pos);
    return true;
}


std::pair<dom::Value, bool>
Handlebars::
evalExpr(
    dom::Value const & context,
    dom::Object const & data,
    dom::Object const & blockValues,
    std::string_view expression) const
{
    expression = trim_spaces(expression);
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
        return {unescapeString(expression.substr(1, expression.size() - 2)), true};
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
        setupArgs(all, context, data, blockValues, args, cb);
        return {fn(args, cb).first, true};
    }
    if (expression.starts_with('@'))
    {
        expression.remove_prefix(1);
        return lookupProperty(data, expression);
    }
    if (expression.starts_with("..")) {
        auto contextPathV = data.find("contextPath");
        if (!contextPathV.isString())
            return {nullptr, false};
        std::string_view contextPath = contextPathV.getString();
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
        dom::Value root = data.find("root");
        do {
            absContextPath = contextPath;
            absContextPath += '.';
            absContextPath += expression;
            auto [v, defined] = lookupProperty(root, absContextPath);
            if (defined) {
                return {v, defined};
            }
            popContextSegment(contextPath);
        } while (!contextPath.empty());
        return lookupProperty(root, expression);
    }
    auto [cv, defined] = lookupProperty(context, expression);
    if (defined) {
        return {cv, defined};
    }
    return lookupProperty(blockValues, expression);
}

auto
Handlebars::
getHelper(std::string_view helper, bool isNoArgBlock) const -> std::pair<helper_type const&, bool> {
    auto it = helpers_.find(helper);
    if (it != helpers_.end()) {
        return {it->second, true};
    }
    helper = !isNoArgBlock ? "helperMissing" : "blockHelperMissing";
    it = helpers_.find(helper);
    MRDOX_ASSERT(it != helpers_.end());
    return {it->second, false};
}

// Parse a block starting at templateText
bool
parseBlock(
    std::string_view blockName,
    Handlebars::Tag const &tag,
    std::string_view &templateText,
    OutputRef &out,
    std::string_view &fnBlock,
    std::string_view &inverseBlock,
    Handlebars::Tag &inverseTag) {
    fnBlock=templateText;
    inverseBlock = {};
    Handlebars::Tag closeTag;
    int l = 1;
    std::string_view* curBlock = &fnBlock;
    while (!templateText.empty())
    {
        std::string_view tagStr;
        if (!findTag(tagStr, templateText))
            break;

        Handlebars::Tag curTag = parseTag(tagStr);

        // move template after the tag
        auto tag_pos = curTag.buffer.data() - templateText.data();
        templateText.remove_prefix(tag_pos + curTag.buffer.size());

        // update section level
        if (!tag.rawBlock) {
            if (curTag.type == '#' || curTag.type2 == '#') {
                // Opening a child section tag
                ++l;
            } else if (curTag.type == '/') {
                // Closing a section tag
                --l;
                if (l == 0) {
                    // Closing the main section tag
                    closeTag = curTag;
                    if (closeTag.content != blockName) {
                        out << fmt::format(R"([mismatched closing tag: "{}" for block name "{}")", closeTag.buffer, blockName);
                        return false;
                    }
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

            // check inversion
            if (l == 1 && curBlock != &inverseBlock) {
                if (curTag.type == '^') {
                    inverseTag = curTag;
                    *curBlock = {curBlock->data(), curTag.buffer.data()};
                    if (curTag.removeLWhitespace) {
                        *curBlock = trim_rspaces(*curBlock);
                    }
                    if (tag.removeRWhitespace) {
                        *curBlock = trim_lspaces(*curBlock);
                    }
                    curBlock = &inverseBlock;
                    *curBlock = templateText;
                    if (curTag.removeRWhitespace) {
                        *curBlock = trim_lspaces(*curBlock);
                        templateText = trim_lspaces(templateText);
                    }
                }
            }
        } else {
            // raw block
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

    // If the first line of fnBlock is only whitespaces, remove it
    // This is a small undocumented detail of handlebars.js that makes
    // the output match what's expected by the users.
    auto posLB = fnBlock.find_first_of("\r\n");
    if (posLB != std::string_view::npos)
    {
        std::string_view first_line = fnBlock.substr(0, posLB);
        if (std::ranges::all_of(first_line, [](char c) { return c == ' '; })) {
            fnBlock.remove_prefix(posLB);
            posLB = fnBlock.find_first_not_of("\r\n");
            if (posLB != std::string_view::npos)
            {
                fnBlock.remove_prefix(posLB);
            }
        }
    }
    // Do the same for the last line
    auto posRB = fnBlock.find_last_of("\r\n");
    if (posRB != std::string_view::npos)
    {
        std::string_view last_line = fnBlock.substr(posRB + 1);
        if (std::ranges::all_of(last_line, [](char c) { return c == ' '; })) {
            fnBlock.remove_suffix(last_line.size());
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
    std::string_view& templateText,
    dom::Value const &data,
    HandlebarsOptions opt,
    partials_map& extra_partials,
    dom::Object const& private_data,
    dom::Object const& blockValues) const {
    if (tag.type == '#')
    {
        renderBlock(tag.helper, tag, out, templateText, data, opt, extra_partials, private_data, blockValues);
    }
    else if (tag.type == '>')
    {
        renderPartial(tag, out, templateText, data, opt, extra_partials, private_data, blockValues);
    }
    else if (tag.type == '*')
    {
        renderDecorator(tag, out, templateText, data, extra_partials, private_data, blockValues);
    }
    else if (tag.type != '/' && tag.type != '!')
    {
        renderExpression(tag, out, templateText, data, opt, private_data, blockValues);
    }
}

void
Handlebars::
renderExpression(
    Handlebars::Tag const &tag,
    OutputRef &out,
    std::string_view &templateText,
    dom::Value const & context,
    HandlebarsOptions const &opt,
    dom::Object const& private_data,
    dom::Object const& blockValues) const
{
    if (tag.helper.empty())
        return;

    auto opt2 = opt;
    opt2.noHTMLEscape = tag.forceNoHTMLEscape || opt.noHTMLEscape;

    auto it = helpers_.find(tag.helper);
    if (it != helpers_.end()) {
        auto [fn, found] = getHelper(tag.helper, false);
        dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
        HandlebarsCallback cb;
        cb.name_ = tag.helper;
        cb.context_ = &context;
        cb.data_ = &private_data;
        cb.logger_ = &logger_;
        setupArgs(tag.arguments, context, private_data, blockValues, args, cb);
        auto [res, render] = fn(args, cb);
        if (render == HelperBehavior::RENDER_RESULT) {
            format_to(out, res, opt2);
        } else if (render == HelperBehavior::RENDER_RESULT_NOESCAPE) {
            opt2.noHTMLEscape = true;
            format_to(out, res, opt2);
        }
        if (tag.removeRWhitespace) {
            templateText = trim_lspaces(templateText);
        }
        return;
    }

    auto [v, defined] = evalExpr(context, private_data, blockValues, tag.helper);
    if (defined)
    {
        format_to(out, v, opt2);
        return;
    }

    // Let it decay to helperMissing hook
    auto [fn, found] = getHelper(tag.helper, false);
    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    HandlebarsCallback cb;
    cb.name_ = tag.helper;
    setupArgs(tag.arguments, context, private_data, blockValues, args, cb);
    auto [res, render] = fn(args, cb);
    if (render == HelperBehavior::RENDER_RESULT) {
        format_to(out, res, opt2);
    } else if (render == HelperBehavior::RENDER_RESULT_NOESCAPE) {
        opt2.noHTMLEscape = true;
        format_to(out, res, opt2);
    }
    if (tag.removeRWhitespace) {
        templateText = trim_lspaces(templateText);
    }
}

void
Handlebars::
setupArgs(
    std::string_view expression,
    dom::Value const& context,
    dom::Object const& data,
    dom::Object const& blockValues,
    dom::Array &args,
    HandlebarsCallback &cb) const
{
    std::string_view expr;
    while (findExpr(expr, expression))
    {
        expression = expression.substr(expr.data() + expr.size() - expression.data());
        auto [k, v] = findKeyValuePair(expr);
        if (k.empty())
        {
            args.emplace_back(evalExpr(context, data, blockValues, expr).first);
            cb.ids_.push_back(expr);
        }
        else
        {
            cb.hashes_.set(k, evalExpr(context, data, blockValues, v).first);
        }
    }
}

void
Handlebars::
renderDecorator(
    Handlebars::Tag const& tag,
    OutputRef &out,
    std::string_view &templateText,
    dom::Value const& context,
    Handlebars::partials_map &extra_partials,
    dom::Object const& data,
    dom::Object const& blockValues) const {
    // Validate decorator
    if (tag.helper != "inline")
    {
        out << fmt::format(R"([undefined decorator "{}" in "{}"])", tag.helper, tag.buffer);
        return;
    }

    // Evaluate expression
    std::string_view expr;
    findExpr(expr, tag.arguments);
    auto [value, defined] = evalExpr(context, data, blockValues, expr);
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
        if (!parseBlock(tag.helper, tag, templateText, out, fnBlock, inverseBlock, inverseTag)) {
            return;
        }
    }
    fnBlock = trim_rspaces(fnBlock);
    extra_partials[std::string(partial_name)] = std::string(fnBlock);
}

void
Handlebars::
renderPartial(
    Handlebars::Tag const &tag,
    OutputRef &out,
    std::string_view &templateText,
    dom::Value const &data,
    HandlebarsOptions &opt,
    Handlebars::partials_map &extra_partials,
    dom::Object const& private_data,
    dom::Object const& blockValues) const {
    // Evaluate dynamic partial
    std::string helper(tag.helper);
    if (helper.starts_with('(')) {
        std::string_view expr;
        findExpr(expr, helper);
        auto [value, defined] = evalExpr(data, private_data, blockValues, expr);
        if (value.isString()) {
            helper = value.getString();
        }
    }

    // Parse block
    std::string_view fnBlock;
    std::string_view inverseBlock;
    Tag inverseTag;
    if (tag.type2 == '#') {
        if (!parseBlock(tag.helper, tag, templateText, out, fnBlock, inverseBlock, inverseTag)) {
            return;
        }
    }

    // Partial
    auto it = this->partials_.find(helper);
    std::string_view partial_content;
    if (it == this->partials_.end())
    {
        it = extra_partials.find(helper);
        if (it == extra_partials.end()) {
            if (tag.type2 == '#') {
                partial_content = fnBlock;
            } else {
                out << "[undefined partial in \"" << tag.buffer << "\"]";
                return;
            }
        } else {
            partial_content = it->second;
        }
    } else {
        partial_content = it->second;
    }

    if (tag.arguments.empty())
    {
        if (tag.type2 == '#') {
            // evaluate fnBlock to populate extra partials
            OutputRef dumb{};
            this->render_to(
                dumb, fnBlock, data, opt,
                extra_partials, private_data, blockValues);
            // also add @partial-block to extra partials
            extra_partials["@partial-block"] = std::string(fnBlock);
        }
        // Render partial with current context
        this->render_to(
            out, partial_content, data, opt,
            extra_partials, private_data, blockValues);
        if (tag.type2 == '#') {
            extra_partials.erase("@partial-block");
        }
    }
    else
    {
        // create context from specified keys
        auto tagContent = tag.arguments;
        dom::Object partialCtx;
        std::string_view expr;
        while (findExpr(expr, tagContent))
        {
            tagContent = tagContent.substr(expr.data() + expr.size() - tagContent.data());
            auto [partialKey, contextKey] = findKeyValuePair(expr);
            if (partialKey.empty())
            {
                auto [value, defined] = evalExpr(data, private_data, blockValues, expr);
                if (defined && value.isObject())
                {
                    partialCtx = createFrame(value.getObject());
                }
                continue;
            }
            if (contextKey != ".") {
                auto [value, defined] = evalExpr(data, private_data, blockValues, contextKey);
                if (defined)
                {
                    partialCtx.set(partialKey, value);
                }
            } else {
                partialCtx.set(partialKey, data);
            }
        }
        this->render_to(
            out, partial_content, partialCtx, opt,
            extra_partials, private_data, blockValues);
    }

    if (tag.removeRWhitespace) {
        templateText = trim_lspaces(templateText);
    }
}

void
Handlebars::
renderBlock(
    std::string_view blockName,
    Handlebars::Tag const &tag,
    OutputRef &out,
    std::string_view &templateText,
    dom::Value const& context,
    HandlebarsOptions const& opt,
    Handlebars::partials_map &extra_partials,
    dom::Object const & data,
    dom::Object const &blockValues) const {
    // Opening a section tag
    // Find closing tag
    if (tag.removeRWhitespace) {
        templateText = trim_lspaces(templateText);
    }

    std::string_view fnBlock;
    std::string_view inverseBlock;
    Tag inverseTag;
    if (!parseBlock(blockName, tag, templateText, out, fnBlock, inverseBlock, inverseTag)) {
        return;
    }

    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    HandlebarsCallback cb;
    cb.name_ = tag.helper;
    cb.context_ = &context;
    cb.data_ = &data;
    cb.logger_ = &logger_;
    cb.output_ = &out;
    bool const isNoArgBlock = tag.arguments.empty();
    auto [fn, found] = getHelper(tag.helper, isNoArgBlock);
    std::string_view arguments = tag.arguments;
    bool const emulateMustache = !found && isNoArgBlock;
    if (emulateMustache) {
        arguments = tag.helper;
    }
    setupArgs(arguments, context, data, blockValues, args, cb);

    // Setup block params
    std::string_view expr;
    std::string_view bps = tag.blockParams;
    while (findExpr(expr, bps))
    {
        bps = bps.substr(expr.data() + expr.size() - bps.data());
        cb.blockParams_.push_back(expr);
    }

    // Setup callback functions
    if (!tag.rawBlock) {
        cb.fn_ = [this, fnBlock, opt, &extra_partials, &blockValues](
            OutputRef os,
            dom::Value const &item,
            dom::Object const &data,
            dom::Object const &newBlockValues) -> void {
            if (!newBlockValues.empty()) {
                dom::Object blockValuesOverlay =
                    createFrame(newBlockValues, blockValues);
                this->render_to(os, fnBlock, item, opt, extra_partials, data, blockValuesOverlay);
            } else {
                this->render_to(os, fnBlock, item, opt, extra_partials, data, blockValues);
            }
        };
        cb.inverse_ =
            [this, inverseTag, inverseBlock, opt, &extra_partials, blockName, &blockValues](
            OutputRef os,
            dom::Value const &item,
            dom::Object const &data,
            dom::Object const &newBlockValues) -> void {
            if (inverseTag.helper.empty()) {
                os << inverseBlock;
                return;
            }
            std::string_view inverseText = inverseBlock;
            if (!newBlockValues.empty()) {
                dom::Object blockValuesOverlay =
                    createFrame(newBlockValues, blockValues);
                renderBlock(blockName, inverseTag, os, inverseText, item, opt, extra_partials, data, blockValuesOverlay);
            } else {
                renderBlock(blockName, inverseTag, os, inverseText, item, opt, extra_partials, data, blockValues);
            }
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

    // Call helper
    auto [res, render] = fn(args, cb);
    if (render == HelperBehavior::RENDER_RESULT) {
        format_to(out, res, opt);
    } else if (render == HelperBehavior::RENDER_RESULT_NOESCAPE) {
        HandlebarsOptions opt2 = opt;
        opt2.noHTMLEscape = true;
        format_to(out, res, opt2);
    }
}

void
Handlebars::
registerPartial(
    std::string_view name,
    std::string_view text)
{
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
            return "unknown";
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

std::string
validateArgs(
    std::string_view helper,
    std::initializer_list<std::initializer_list<dom::Kind>> il,
    dom::Array const& args) {
    auto r = validateArgs(helper, il.size(), args);
    if (!r.empty()) {
        return r;
    }
    for (size_t i = 0; i < args.size(); ++i) {
        auto allowed = il.begin()[i];
        bool const any_valid = std::ranges::any_of(allowed, [&](dom::Kind a) {
            return args[i].kind() == a;
        });
        if (!any_valid) {
            std::string allowed_str(kindToString(allowed.begin()[0]));
            for (auto a : std::views::drop(allowed, 1)) {
                allowed_str += ", or ";
                allowed_str += kindToString(a);
            }
            return fmt::format(R"(["{}" helper requires argument {} to be of type {}: {} provided])", helper, i, allowed_str, kindToString(args[i].kind()));
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
        auto [v, defined] = lookupProperty(ctx, "data.root.page");
        if (v.isString()) {
            from = v.getString();
        } else {
            std::tie(v, defined) = lookupProperty(ctx, "data.root.site.path");
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
    return lookupProperty(obj, field).first;
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

} // helpers

} // mrdox
} // clang

