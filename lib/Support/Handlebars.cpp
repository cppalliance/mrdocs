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

namespace clang {
namespace mrdox {

bool
isTruthy(llvm::json::Value const& arg)
{
    if (arg.kind() == llvm::json::Value::Kind::Number)
        return arg.getAsInteger().value() != 0;
    if (arg.kind() == llvm::json::Value::Kind::Boolean)
        return arg.getAsBoolean().value();
    if (arg.kind() == llvm::json::Value::Kind::String)
        return !arg.getAsString().value().empty();
    if (arg.kind() == llvm::json::Value::Kind::Array)
        return !arg.getAsArray()->empty();
    if (arg.kind() == llvm::json::Value::Kind::Object)
        return !arg.getAsObject()->empty();
    else
        return false;
};

std::string
Handlebars::
render(
    std::string_view templateText,
    llvm::json::Object const &data,
    HandlebarsOptions opt) const
{
    std::string out;
    llvm::raw_string_ostream os(out);
    render_to(
        os,
        templateText,
        data,
        opt);
    return out;
}

// Find the next handlebars tag
// Returns true if found, false if not
// If found, tag is set to the tag text
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

    char type = '\0';

    // From after type until closing tag
    std::string_view content;

    // First expression in content if more than one expression
    std::string_view helper;

    // Other expressions in content
    std::string_view expression;

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

// Find next expression in tag content
bool
findExpr(std::string_view & expr, std::string_view tagContent)
{
    tagContent = trim_spaces(tagContent);
    if (tagContent.empty())
        return false;

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
        t.expression = tagStr;
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
        t.expression = tagStr.substr(0);
        return t;
    }

    // Find tag type
    static constexpr std::array<char, 5> tag_types({'#', '/', '>', '!'});
    auto it = std::ranges::find_if(tag_types, [tagStr](char c) { return c == tagStr.front(); });
    if (it != tag_types.end()) {
        t.type = tagStr.front();
        tagStr.remove_prefix(1);
        tagStr = trim_spaces(tagStr);
    } else if (t.rawBlock) {
        t.type = '#';
    } else {
        t.type = ' ';
    }
    t.content = tagStr;

    // Find helper and expression
    std::string_view expr;
    std::string_view tagContent = t.content;
    if (t.type == '#' || t.type == '>') {
        // Tag type requires a helper
        // First expression is always a helper even if there's only one expression
        if (findExpr(expr, tagContent)) {
            t.helper = expr;
            tagContent.remove_prefix(expr.data() + expr.size() - tagContent.data());
            t.expression = trim_spaces(tagContent);
        } else {
            t.helper = tagContent;
            t.expression = {};
        }
    } else {
        // Tag type doesn't require a helper
        // First expression is always an expression even if there's only one expression
        // If there's a second expression, the first is a helper
        if (findExpr(expr, tagContent)) {
            t.expression = expr;
            t.helper = {};
        }
        tagContent.remove_prefix(expr.data() + expr.size() - tagContent.data());
        if (findExpr(expr, tagContent)) {
            // Second expression exists, so first is a helper and the rest is the arguments
            t.helper = t.expression;
            t.expression = {expr.data(), static_cast<std::size_t>(tagContent.data() + tagContent.size() - expr.data())};
        }
    }
    return t;
}

void
Handlebars::
render_to(
    llvm::raw_string_ostream& out,
    std::string_view templateText,
    llvm::json::Object const &data,
    HandlebarsOptions opt) const
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
            auto pos = templateText.substr(0, tagStartPos).find_last_not_of(" \t\r\n");
            if (pos != std::string_view::npos) {
                templateEndPos = pos + 1;
            } else {
                templateEndPos = 0;
            }
        }
        out << templateText.substr(0, templateEndPos);
        templateText.remove_prefix(tagStartPos + tagStr.size());
        if (!tag.escaped) {
            renderTag(tag, out, templateText, data, opt);
        } else {
            out << tag.content;
        }
    }
}

void
escape_to(
    llvm::raw_string_ostream& out,
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
    llvm::raw_string_ostream& out,
    llvm::json::Value const& value,
    HandlebarsOptions opt)
{
    namespace stdr = std::ranges;
    namespace stdv = stdr::views;
    if (value.kind() == llvm::json::Value::Kind::String) {
        escape_to(out, *value.getAsString(), opt);
    } else if (value.kind() == llvm::json::Value::Kind::Number) {
        out << *value.getAsNumber();
    } else if (value.kind() == llvm::json::Value::Kind::Boolean) {
        out << (value.getAsBoolean() ? "true" : "false");
    } else if (value.kind() == llvm::json::Value::Kind::Array) {
        out << "[";
        llvm::json::Array const& array = *value.getAsArray();
        if (!array.empty()) {
            format_to(out, array[0], opt);
            for (auto const &item : stdv::drop(array, 1)) {
                out << " ,";
                format_to(out, item, opt);
            }
        }
        out << "]";
    } else if (value.kind() == llvm::json::Value::Kind::Object) {
        out << "{";
        llvm::json::Object const& object = *value.getAsObject();
        if (!object.empty()) {
            out << "\"" << object.begin()->first << "\": ";
            format_to(out, object.begin()->second, opt);
            for (auto const &item : stdv::drop(object, 1)) {
                out << " , \"" << item.first << "\": ";
                format_to(out, item.second, opt);
            }
        }
        out << "}";
    } else if (value.kind() != llvm::json::Value::Kind::Null) {
        out << value;
    }
}

llvm::json::Value const*
findNested(
    llvm::json::Object const &data,
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
    auto data_it = data.find(segment);
    if (data_it == data.end())
        return nullptr;
    llvm::json::Value const* cur = &data_it->second;

    // Recursively get more values from Values
    std::tie(segment, path) = nextSegment(path);
    while (!segment.empty())
    {
        // If current value is an Object, get the next value from it
        if (cur->getAsObject())
        {
            auto obj = cur->getAsObject();
            auto it = obj->find(segment);
            if (it == obj->end())
                return nullptr;
            cur = &it->second;
        }
        // If current value is an Array, get the next value the stripped index
        else if (cur->getAsArray())
        {
            size_t index;
            std::from_chars_result res = std::from_chars(
                segment.data(),
                segment.data() + segment.size(),
                index);
            if (res.ec != std::errc())
                return nullptr;
            auto& arr = *cur->getAsArray();
            if (index >= arr.size())
                return nullptr;
            cur = &arr[index];
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
    auto pos = expression.find_first_of("=");
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

llvm::json::Value
Handlebars::
evalExpr(
    llvm::json::Object const &data,
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
    else if (expression == ".")
    {
        return llvm::json::Object(data);
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
            return 0;
        return value;
    }
    else if (expression.starts_with('(') && expression.ends_with(')'))
    {
        std::string_view all = expression.substr(1, expression.size() - 2);
        std::string_view helper;
        findExpr(helper, all);
        auto it = helpers_.find(helper);
        if (it == helpers_.end())
        {
            return fmt::format(R"([undefined helper "{}" from "{}"])", helper, all);
        }
        helper_type const& fn = it->second;

        all.remove_prefix(helper.data() + helper.size() - all.data());
        std::string_view sub;
        llvm::json::Array args;
        HandlebarsCallback cb;
        while (findExpr(sub, all)) {
            all.remove_prefix(sub.end() - all.begin());
            auto [k, v] = findKeyValuePair(sub);
            if (k.empty())
            {
                args.push_back(evalExpr(data, sub));
            }
            else
            {
                cb.hashes.try_emplace(llvm::json::ObjectKey(k), evalExpr(data, v));
            }
        }
        return fn(data, args, cb);
    }
    else
    {
        llvm::json::Value const* value = findNested(data, expression);
        if (value != nullptr)
        {
            return *value;
        }
        return nullptr;
    }
}

// Render a handlebars tag
void
Handlebars::
renderTag(
    Tag const& tag,
    llvm::raw_string_ostream& out,
    std::string_view& templateText,
    llvm::json::Object const &data,
    HandlebarsOptions opt) const {
    if (tag.type == '#')
    {
        // Opening a section tag
        // Find closing tag
        Tag closeTag;
        int l = 1;
        if (tag.removeRWhitespace) {
            templateText = trim_lspaces(templateText);
        }
        std::string_view fnBlock = templateText;
        std::string_view inverseBlock;
        std::string_view* curBlock = &fnBlock;
        while (!templateText.empty())
        {
            std::string_view tagStr;
            if (!findTag(tagStr, templateText))
                break;

            Tag curTag = parseTag(tagStr);

            // move template after the tag
            auto tag_pos = curTag.buffer.data() - templateText.data();
            templateText.remove_prefix(tag_pos + curTag.buffer.size());

            // update section level
            if (!tag.rawBlock) {
                if (curTag.type == '#') {
                    // Opening a child section tag
                    ++l;
                } else if (curTag.type == '/') {
                    // Closing a section tag
                    --l;
                    if (l == 0) {
                        // Closing the main section tag
                        closeTag = curTag;
                        if (closeTag.content != tag.helper) {
                            out << "[mismatched closing tag: " << closeTag.buffer << "]";
                            return;
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
                if (l == 1) {
                    if (curTag.content == "^" || curTag.content == "else") {
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
                if (curTag.type == '/' && tag.rawBlock == curTag.rawBlock && tag.helper == curTag.content) {
                    // Closing the raw section
                    l = 0;
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

        // Setup helper arguments
        // 1st argument) context
        llvm::json::Object const& context = data;

        // 2nd argument) array of block items as the args
        // In general, the args should be an array (eg. {{#each items}}), but
        // some special helpers may use a single value (eg. {{#if item}}).
        // When the args is a single value, we wrap it in a single element
        // array.
        llvm::json::Array args;
        llvm::json::Value value = evalExpr(data, tag.expression);
        if (!value.getAsArray())
        {
            args.push_back(std::move(value));
        }
        else if (value.getAsArray())
        {
            llvm::json::Array* valueArray = value.getAsArray();
            args = std::move(*valueArray);
        }

        // 3rd argument) callbacks
        HandlebarsCallback cb;
        if (!tag.rawBlock) {
            cb.fn = [this, fnBlock, opt](llvm::json::Object const &item) -> std::string {
                // Render one element in the list with the fnBlock template
                return render(fnBlock, item, opt);
            };
            cb.inverse = [this, inverseBlock, opt](llvm::json::Object const &item) -> std::string {
                // Render one element in the list with the inverseBlock template
                return render(inverseBlock, item, opt);
            };
        } else {
            cb.fn = [this, fnBlock, opt](llvm::json::Object const &item) -> std::string {
                // Render one element in the list with the fnBlock template
                return std::string(fnBlock);
            };
            cb.inverse = [this, inverseBlock, opt](llvm::json::Object const &item) -> std::string {
                // Render one element in the list with the inverseBlock template
                return {};
            };
        }

        // Call helper
        auto it = helpers_.find(tag.helper);
        if (!tag.rawBlock) {
            if (it == helpers_.end())
            {
                out << "[undefined helper in \"" << tag.buffer << "\"]";
                return;
            }
            helper_type const& fn = it->second;
            llvm::json::Value res = fn(context, args, cb);
            HandlebarsOptions opt2 = opt;
            opt2.noHTMLEscape = true;
            format_to(out, res, opt2);
        } else {
            llvm::json::Value res(nullptr);
            if (it == helpers_.end())
            {
                res = {fnBlock};
            } else {
                helper_type const& fn = it->second;
                res = fn(context, args, cb);
            }
            HandlebarsOptions opt2 = opt;
            opt2.noHTMLEscape = true;
            format_to(out, res, opt2);
        }
    }
    else if (tag.type == '/' || tag.type == '!')
    {
        // Comment or orphan section closing tag
        // This will handle both {{!}} and {{!----}} comments
        // End a section here represents an error in the template, since we
        // should have already closed the section in the first condition
    }
    else if (tag.type == '>')
    {
        // Partial
        auto it = partials_.find(tag.helper);
        if (it == partials_.end())
        {
            out << "[undefined partial in \"" << tag.buffer << "\"]";
            return;
        }

        if (tag.expression.empty())
        {
            // inherit context
            render_to(out, it->second, data, opt);
        }
        else
        {
            // create context from specified keys
            auto tagContent = tag.expression;
            llvm::json::Object partialCtx;
            std::string_view expr;
            while (findExpr(expr, tagContent))
            {
                tagContent = tagContent.substr(expr.data() + expr.size() - tagContent.data());
                auto [partialKey, contextKey] = findKeyValuePair(expr);
                if (partialKey.empty())
                {
                    continue;
                }
                if (contextKey != ".") {
                    llvm::json::Value const* value = findNested(data, contextKey);
                    if (value != nullptr)
                    {
                        partialCtx.try_emplace({partialKey}, *value);
                    }
                } else {
                    partialCtx.try_emplace({partialKey}, llvm::json::Value(llvm::json::Object(data)));
                }
            }
            render_to(out, it->second, partialCtx, opt);
        }

        if (tag.removeRWhitespace) {
            templateText = trim_lspaces(templateText);
        }
    }
    else
    {
        // Expression
        auto opt2 = opt;
        if (tag.forceNoHTMLEscape)
        {
            // Unescaped tag content
            opt2.noHTMLEscape = true;
        }

        if (tag.content.empty())
        {
            // No helper, no expression: nothing to render
            return;
        }

        if (tag.helper.empty() && !tag.expression.empty()) {
            llvm::json::Value v = evalExpr(data, tag.expression);
            format_to(out, v, opt2);
            return;
        }

        // Multiple expressions, first string is a helper function
        auto it = helpers_.find(tag.helper);
        if (it == helpers_.end())
        {
            out << "[undefined helper in \"" << tag.buffer << "\"]";
            return;
        }
        helper_type const& fn = it->second;

        // Remaining expressions are a list of arguments to the helper function
        // Each argument may be an argument to the function or a key-value pair
        // for the callback hashes
        llvm::json::Array args;
        HandlebarsCallback cb;
        std::string_view tagContent = tag.expression;
        std::string_view expr;
        while (findExpr(expr, tagContent))
        {
            tagContent = tagContent.substr(expr.data() + expr.size() - tagContent.data());
            auto [k, v] = findKeyValuePair(expr);
            if (k.empty())
            {
                args.push_back(evalExpr(data, expr));
            }
            else
            {
                cb.hashes.try_emplace(llvm::json::ObjectKey(k), evalExpr(data, v));
            }
        }

        llvm::json::Value res = fn(data, args, cb);
        format_to(out, res, opt2);

        if (tag.removeRWhitespace) {
            templateText = trim_lspaces(templateText);
        }
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
registerHelper(
    std::string_view name,
    helper_type const &helper)
{
    helpers_.emplace(std::string(name), helper);
}

namespace helpers {

void
registerStandardHelpers(Handlebars& hbs)
{
    hbs.registerHelper("if", if_fn);
    hbs.registerHelper("unless", unless_fn);
    hbs.registerHelper("with", with_fn);
    hbs.registerHelper("each", each_fn);
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

// Load helpers
// These helpers implement all the Handlebars functionality as
// described in https://handlebarsjs.com/guide/builtin-helpers.html
namespace stdr = std::ranges;
namespace stdv = std::ranges::views;
namespace json = llvm::json;

llvm::json::Value
and_fn(llvm::json::Array const& args) {
    return stdr::all_of(args, isTruthy);
}

llvm::json::Value
or_fn(llvm::json::Array const& args) {
    return stdr::any_of(args, isTruthy);
}

llvm::json::Value
eq_fn(llvm::json::Array const& args) {
    return stdr::all_of(stdv::drop(args, 1),
        [&args](const llvm::json::Value &arg) {
            return arg == args[0];
    });
}

llvm::json::Value
ne_fn(llvm::json::Array const& args) {
    return !stdr::all_of(stdv::drop(args, 1),
        [&args](const llvm::json::Value &arg) {
            return arg == args[0];
    });
}

llvm::json::Value
not_fn(
    llvm::json::Value const& arg) {
    return !isTruthy(arg);
}

llvm::json::Value
increment_fn(
    llvm::json::Value const &arg) {
    if (arg.kind() == llvm::json::Value::Kind::Number)
        return arg.getAsNumber().value() + 1;
    if (arg.kind() == llvm::json::Value::Kind::Boolean)
        return true;
    else
        return arg;
}

llvm::json::Value
detag_fn(
    std::string_view html) {
    // equivalent to:
    // static const std::regex tagRegex("<[^>]+>");
    // return std::regex_replace(html, tagRegex, "");
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

llvm::json::Value
relativize_fn(
    llvm::json::Object const& ctx0,
    llvm::json::Array const& args) {
    if (args.empty()) {
        return "#";
    }
    // Destination path
    llvm::StringRef to = args[0].getAsString().value();
    if (!isTruthy(to)) {
        return "#";
    }
    if (to.front() != '/') {
        return to;
    }
    // Source path
    llvm::StringRef from;
    llvm::json::Object const* ctx = &ctx0;
    if (args.size() > 1)
    {
        if (args[1].kind() == llvm::json::Value::Kind::String)
            from = args[1].getAsString().value();
        else if (args[1].kind() == llvm::json::Value::Kind::Object)
            ctx = args[1].getAsObject();
    }
    if (from.empty()) {
        llvm::json::Object const* obj;
        obj = ctx->getObject("data");
        if (obj) {
            obj = obj->getObject("root");
            if (obj)
            {
                obj = obj->getObject("page");
                if (obj)
                {
                    from = obj->getString("page").value();
                }
            }
        }
    }
    if (from.empty()) {
        llvm::json::Object const* obj;
        obj = ctx->getObject("data");
        if (obj) {
            obj = obj->getObject("root");
            if (obj)
            {
                obj = obj->getObject("site");
                if (obj)
                {
                    auto optFrom = obj->getString("path");
                    if (optFrom)
                    {
                        from = *optFrom;
                    }
                    std::string fromStr(from);
                    fromStr += to;
                    return fromStr;
                }
            }
        }
    }
    auto hashIdx = stdr::find(to, '#') - to.begin();
    llvm::StringRef hash = to.substr(hashIdx);
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

llvm::json::Value
year_fn() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&currentTime);
    int year = localTime->tm_year + 1900;
    return year;
}

// https://github.com/handlebars-lang/handlebars.js/tree/master/lib/handlebars/helpers
llvm::json::Value
if_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& conditional,
    HandlebarsCallback const& cb) {
    // The if block helper has a special logic where is uses the first
    // argument as a conditional but uses the content itself as the
    // item to render the callback
    if (!conditional.empty() && isTruthy(conditional[0])) {
        return cb.fn(context);
    } else {
        return cb.inverse(context);
    }
}

llvm::json::Value
unless_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& conditional,
    HandlebarsCallback const& cb) {
    // The unless block helper has a special logic where is uses the first
    // argument as a conditional but uses the content itself as the
    // item to render the callback
    if (!conditional.empty() && isTruthy(conditional[0])) {
        return cb.inverse(context);
    } else {
        return cb.fn(context);
    }
}

llvm::json::Value
with_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb) {
    // Built-in helper to change the context
    if (!args.empty() && args[0].kind() == llvm::json::Value::Kind::Object) {
        llvm::json::Object frame = *args[0].getAsObject();
        frame.try_emplace("..", llvm::json::Object(context));
        return cb.fn(frame);
    }
    return cb.inverse(context);
}

llvm::json::Value
each_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb) {
    // Built-in helper to change the context for each object in args
    if (!args.empty()) {
        std::string out;
        for (auto const& arg : args) {
            if (arg.kind() == llvm::json::Value::Kind::Object) {
                llvm::json::Object frame = *arg.getAsObject();
                frame.try_emplace("..", llvm::json::Object(context));
                out += cb.fn(frame);
            }
        }
        return out;
    }
    return cb.inverse(context);
}

}

} // mrdox
} // clang
