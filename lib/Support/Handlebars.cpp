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
        : parent_(parent)
    {}

    OverlayObjectImpl(dom::Object child, dom::Object parent)
        : parent_(parent)
        , child_(child)
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

    void set(std::string_view key, dom::Value value) override {
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

void
escape_to(
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
}

void
format_to(
    OutputRef out,
    dom::Value const& value,
    HandlebarsOptions opt)
{
    namespace stdr = std::ranges;
    namespace stdv = stdr::views;
    if (value.isString()) {
        escape_to(out, value.getString(), opt);
    } else if (value.kind() == dom::Kind::Integer) {
        out << value.getInteger();
    } else if (value.kind() == dom::Kind::Boolean) {
        if (value.getBool()) {
            out << "true";
        } else {
            out << "false";
        }
    } else if (value.kind() == dom::Kind::Array) {
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
    } else if (value.kind() == dom::Kind::Object) {
        out << "[object Object]";
    } else if (value.kind() != dom::Kind::Null) {
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
log(dom::Value const& level0,
    dom::Array const& args) const {
    dom::Value level = level0;
    if (level.isString()) {
        std::string_view levelStr = level.getString();
        if (levelStr == "debug") {
            level = std::int64_t(0);
        } else if (levelStr == "info") {
            level = 1;
        } else if (levelStr == "warn") {
            level = 2;
        } else if (levelStr == "error") {
            level = 3;
        } else {
            int levelInt = 0;
            auto res = std::from_chars(
                levelStr.data(),
                levelStr.data() + levelStr.size(),
                levelInt);
            if (res.ec == std::errc()) {
                level = levelInt;
            } else {
                level = std::int64_t(0);
            }
        }
    }

    if (!level.getInteger()) {
        return;
    }

    // AFREITAS: handlebars should propagate its log level to the callback
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/lib/handlebars/helpers/log.js
    // https://github.com/handlebars-lang/handlebars.js/blob/master/lib/handlebars/logger.js
    static constexpr int hbs_log_level = 4;

    if (level.getInteger() < hbs_log_level) {
        std::string out;
        OutputRef os(out);
        for (std::size_t i = 0; i < args.size(); ++i) {
            HandlebarsOptions opt;
            opt.noHTMLEscape = true;
            format_to(os, args.at(i), opt);
            os << " ";
        }
        fmt::println("{}", out);
    }
}

/////////////////////////////////////////////////////////////////
// Engine
/////////////////////////////////////////////////////////////////

Handlebars::Handlebars() {
    helpers::registerBuiltinHelpers(*this);
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

dom::Value
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
        return nullptr;
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
                return nullptr;
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
                return nullptr;
            auto& arr = cur.getArray();
            if (index >= arr.size())
                return nullptr;
            cur = arr.at(index);
        }
        else
        {
            // Current value is not an Object or Array, so we can't get any more
            // segments from it
            return nullptr;
        }
        // Consume more segments to get into the array element
        std::tie(segment, path) = nextSegment(path);
    }
    return cur;
}

dom::Value
lookupProperty(
    dom::Value const &data,
    std::string_view path) {
    if (path == "." || path == "this" || path == "[.]" || path == "[this]" || path.empty())
        return data;
    if (data.kind() != dom::Kind::Object) {
        return nullptr;
    }
    return lookupProperty(data.getObject(), path);
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
        return fmt::format("\"{}\"", value.getString());
    case dom::Kind::Array:
    {
        dom::Array arr = value.getArray();
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
        dom::Object obj = value.getObject();
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
                s += fmt::format("\"{}\":{}", item.key, JSON_stringify(item.value, done));
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
appendContextPath(dom::Value contextPath, std::string_view id) {
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


dom::Value
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
        return true;
    }
    else if (is_literal_value(expression, "false"))
    {
        return false;
    }
    if (is_literal_value(expression, "null") || is_literal_value(expression, "undefined") || expression.empty())
    {
        return nullptr;
    }
    else if (expression == "." || expression == "this")
    {
        if (context.isObject() && context.getObject().exists("this"))
            return context.getObject().find("this");
        return context;
    }
    else if (is_literal_string(expression))
    {
        return unescapeString(expression.substr(1, expression.size() - 2));
    }
    else if (is_literal_integer(expression))
    {
        std::int64_t value;
        auto res = std::from_chars(
            expression.data(),
            expression.data() + expression.size(),
            value);
        if (res.ec != std::errc())
            return std::int64_t(0);
        return value;
    }
    else if (expression.starts_with('(') && expression.ends_with(')'))
    {
        std::string_view all = expression.substr(1, expression.size() - 2);
        std::string_view helper;
        findExpr(helper, all);
        helper_type const& fn = getHelper(helper, false);
        all.remove_prefix(helper.data() + helper.size() - all.data());
        dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
        HandlebarsCallback cb;
        cb.name_ = helper;
        cb.context_ = &context;
        setupArgs(all, context, data, blockValues, args, cb);
        return fn(args, cb);
    }
    else
    {
        if (expression.starts_with('@'))
        {
            expression.remove_prefix(1);
            return lookupProperty(data, expression);
        }
        if (expression.starts_with("..")) {
            auto contextPathV = data.find("contextPath");
            if (!contextPathV.isString())
                return nullptr;
            std::string_view contextPath = contextPathV.getString();
            while (expression.starts_with("..")) {
                if (!popContextSegment(contextPath))
                    return nullptr;
                expression.remove_prefix(2);
                if (!expression.empty()) {
                    if (expression.front() != '/') {
                        return nullptr;
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
                dom::Value v = lookupProperty(root, absContextPath);
                if (!v.isNull()) {
                    return v;
                }
                popContextSegment(contextPath);
            } while (!contextPath.empty());
            return lookupProperty(root, expression);
        }
        dom::Value cv = lookupProperty(context, expression);
        if (!cv.isNull()) {
            return cv;
        }
        return lookupProperty(blockValues, expression);
    }
}

auto
Handlebars::
getHelper(std::string_view helper, bool isNoArgBlock) const -> helper_type const& {
    auto it = helpers_.find(helper);
    if (it != helpers_.end()) {
        return it->second;
    }
    // This hook is called when a mustache or a block-statement
    // - a simple mustache-expression is not a registered helper AND
    // - is not a property of the current evaluation context.
    // you can add custom handling for those situations by registering the helper helperMissing
    // The helper receives
    // the same arguments and options (hash, name, etc.) as any custom helper
    // or block-helper. The options.name is the name of the helper being called.
    // If no parameters are passed to the mustache, the default behavior is to do nothing
    // and ignore the whole mustache expression or the whole block (noop_fn)
    it = helpers_.find(!isNoArgBlock ? "helperMissing" : "blockHelperMissing");
    if (it != helpers_.end()) {
        return it->second;
    }
    if (it == helpers_.end())
    {
        thread_local static helper_type h = [](
                dom::Array const &args,
                HandlebarsCallback const &cb) -> dom::Value {
            return fmt::format(R"([undefined helper "{}"])", cb.name());
        };
        return h;
    }
    thread_local static helper_type noop_v = helpers::noop_fn;
    return noop_v;
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
        helper_type const& fn = getHelper(tag.helper, false);
        dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
        HandlebarsCallback cb;
        cb.name_ = tag.helper;
        cb.context_ = &context;
        cb.data_ = private_data;
        setupArgs(tag.arguments, context, private_data, blockValues, args, cb);
        dom::Value res = fn(args, cb);
        format_to(out, res, opt2);
        if (tag.removeRWhitespace) {
            templateText = trim_lspaces(templateText);
        }
        return;
    }

    dom::Value v = evalExpr(context, private_data, blockValues, tag.helper);
    if (!v.isNull())
    {
        format_to(out, v, opt2);
        return;
    }

    // Let it decay to helperMissing hook
    helper_type const& fn = getHelper(tag.helper, false);
    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    HandlebarsCallback cb;
    cb.name_ = tag.helper;
    setupArgs(tag.arguments, context, private_data, blockValues, args, cb);
    dom::Value res = fn(args, cb);
    format_to(out, res, opt2);
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
            args.emplace_back(this->evalExpr(context, data, blockValues, expr));
            cb.ids_.push_back(expr);
        }
        else
        {
            cb.hashes_.set(k, this->evalExpr(context, data, blockValues, v));
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
    dom::Value value = this->evalExpr(context, data, blockValues, expr);
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
    // continue as usual with this new extra partial
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
        dom::Value value = this->evalExpr(data, private_data, blockValues, expr);
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
                dom::Value value = evalExpr(data, private_data, blockValues, expr);
                if (!value.isNull() && value.isObject())
                {
                    partialCtx = value.getObject();
                }
                continue;
            }
            if (contextKey != ".") {
                dom::Value value = evalExpr(data, private_data, blockValues, contextKey);
                if (!value.isNull())
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
    cb.data_ = data;
    setupArgs(tag.arguments, context, data, blockValues, args, cb);

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
    helper_type const& fn = getHelper(tag.helper, args.empty());
    dom::Value res = fn(args, cb);
    HandlebarsOptions opt2 = opt;
    opt2.noHTMLEscape = true;
    format_to(out, res, opt2);
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
registerHelper(
    std::string_view name,
    helper_type const &helper)
{
    helpers_.emplace(std::string(name), helper);
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

dom::Value
and_fn(dom::Array const& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!isTruthy(args[i])) {
            return false;
        }
    }
    return true;
}

dom::Value
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
    for (std::size_t i = 0; i < a.size(); ++i) {
        std::string_view k = a[i].key;
        if (!b.exists(k)) {
            return false;
        }
        if (!isSame(a[i].value, b.find(k))) {
            return false;
        }
    }
    return true;
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

dom::Value
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

dom::Value
ne_fn(dom::Array const& args) {
    return !eq_fn(args).getBool();
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

dom::Value
not_fn(dom::Array const& args) {
    return !isTruthy(args[0]);
}

dom::Value
increment_fn(
    dom::Array const &args,
    HandlebarsCallback const& cb)
{
    auto r = validateArgs(cb.name(), 1, args);
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

dom::Value
detag_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb)
{
    // equivalent to:
    // static const std::regex tagRegex("<[^>]+>");
    // return std::regex_replace(html, tagRegex, "");
    auto r = validateArgs(cb.name(), {dom::Kind::String}, args);
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
    HandlebarsCallback const& cb) {
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
    dom::Value ctx = cb.context();
    if (args.size() > 1)
    {
        if (args[1].kind() == dom::Kind::String)
            from = args[1].getString();
        else if (args[1].kind() == dom::Kind::Object)
            ctx = args[1];
    }

    if (from.empty() && ctx.isObject()) {
        dom::Value v = lookupProperty(ctx, "data.root.page");
        if (v.isString()) {
            from = v.getString();
        } else {
            v = lookupProperty(ctx, "data.root.site.path");
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

dom::Value
year_fn() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&currentTime);
    int year = localTime->tm_year + 1900;
    return year;
}

dom::Value
if_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb) {
    // The if block helper has a special logic where is uses the first
    // argument as a conditional but uses the content itself as the
    // item to render the callback
    auto r = validateArgs(cb.name(), 1, args);
    if (!r.empty()) {
        return r;
    }
    dom::Value const& conditional = args[0];
    if (conditional.kind() == dom::Kind::Integer) {
        // Treat the zero path separately
        std::int64_t v = conditional.getInteger();
        if (v == 0) {
            bool includeZero = false;
            if (cb.hashes().exists("includeZero")) {
                auto zeroV = cb.hashes().find("includeZero");
                if (zeroV.isBoolean()) {
                    includeZero = zeroV.getBool();
                }
            }
            if (includeZero) {
                return cb.fn();
            } else {
                return cb.inverse();
            }
        }
    }
    if (isTruthy(conditional)) {
        return cb.fn();
    }
    return cb.inverse();
}

dom::Value
unless_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb) {
    // Same as "if". Swap inverse and fn.
    auto r = validateArgs(cb.name(), 1, args);
    if (!r.empty()) {
        return r;
    }
    dom::Value const& conditional = args[0];
    if (conditional.kind() == dom::Kind::Integer) {
        // Treat the zero path separately
        std::int64_t v = conditional.getInteger();
        if (v == 0) {
            bool includeZero = false;
            if (cb.hashes().exists("includeZero")) {
                auto zeroV = cb.hashes().find("includeZero");
                if (zeroV.isBoolean()) {
                    includeZero = zeroV.getBool();
                }
            }
            if (includeZero) {
                return cb.inverse();
            } else {
                return cb.fn();
            }
        }
    }
    if (isTruthy(conditional)) {
        return cb.inverse();
    }
    return cb.fn();
}

dom::Value
with_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb) {
    // Built-in helper to change the context
    auto r = validateArgs(cb.name(), 1, args);
    if (!r.empty()) {
        return r;
    }

    dom::Value newContext = args[0];
    if (!isEmpty(newContext)) {
        dom::Object data = createFrame(cb.data());
        std::string contextPath = appendContextPath(
              cb.data().find("contextPath"), cb.ids()[0]);
        data.set("contextPath", contextPath);
        dom::Object blockValues;
        if (!cb.blockParams().empty()) {
            blockValues.set(cb.blockParams()[0], newContext);
        }
        return cb.fn(newContext, data, blockValues);
    }
    return cb.inverse();
}

dom::Value
each_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb) {
    auto r = validateArgs(cb.name(), 1, args);
    if (!r.empty()) {
        return r;
    }

    MRDOX_ASSERT(!cb.ids().empty());
    std::string contextPath = appendContextPath(
        cb.data().find("contextPath"), cb.ids()[0]) + '.';

    dom::Object data = createFrame(cb.data());
    dom::Object blockValues;

    std::string out;
    dom::Value context = args[0];
    if (context.isArray()) {
        dom::Array items = context.getArray();
        for (std::size_t index = 0; index < items.size(); ++index) {
            dom::Value item = items.at(index);

            // Private values frame
            data.set("key", index);
            data.set("index", index);
            data.set("first", index == 0);
            data.set("last", index == items.size() - 1);
            data.set("contextPath", contextPath + std::to_string(index));

            // Block values
            if (cb.blockParams().size() > 0) {
                blockValues.set(cb.blockParams()[0], item);
                if (cb.blockParams().size() > 1) {
                    blockValues.set(cb.blockParams()[1], index);
                }
            }

            out += cb.fn(item, data, blockValues);
        }
    } else if (context.kind() == dom::Kind::Object) {
        dom::Object items = context.getObject();
        for (std::size_t index = 0; index < items.size(); ++index) {
            dom::Object::reference item = items[index];

            // Private values frame
            data.set("key", item.key);
            data.set("index", index);
            data.set("first", index == 0);
            data.set("last", index == items.size() - 1);
            data.set("contextPath", contextPath + std::string(item.key));

            // Block values
            if (cb.blockParams().size() > 0) {
                blockValues.set(cb.blockParams()[0], item.value);
                if (cb.blockParams().size() > 1) {
                    blockValues.set(cb.blockParams()[1], item.key);
                }
            }

            out += cb.fn(item.value, data, blockValues);
        }
    } else {
        out += cb.inverse();
    }
    return out;
}

dom::Value
lookup_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb) {
    auto r = validateArgs(cb.name(), 2, args);
    if (!r.empty()) {
        return r;
    }
    if (args[1].isString()) {
        // Second argument is a string, so we're looking up a property
        std::string_view key = args[1].getString();
        dom::Value v = lookupProperty(args[0], key);
        if (!v.isNull()) {
            return v;
        }
    } else if (args[1].isInteger()) {
        // Second argument is a number, so we're looking up an array index
        if (args[0].isArray()) {
            dom::Array a = args[0].getArray();
            auto indexR = args[1].getInteger();
            auto index = static_cast<std::size_t>(indexR);
            if (index < a.size()) {
                return a[index];
            }
        }
    }
    return nullptr;
}

void
log_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb) {
    dom::Value level = cb.hashes().find("level");
    if (level.isNull()) {
        level = 1;
    }
    cb.log(level, args);
}

dom::Value
noop_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb) {
    if (cb.isBlock()) {
        // If the hook is not overridden, then the default implementation will
        // mimic the behavior of Mustache and just render the block.
        return cb.fn();
    }
    if (args.empty()) {
        return nullptr;
    }
    return fmt::format(R"(Missing helper: "{}")", cb.name());
}

} // helpers

} // mrdox
} // clang

