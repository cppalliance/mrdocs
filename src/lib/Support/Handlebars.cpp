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
    // ==========================================
    // No indent
    // ==========================================
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

    // ==========================================
    // Indented
    // ==========================================
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
        fptr_( out_, sv.substr(pos, (next - pos) + 1) );
        pos = next + 1;
    }
    return *this;
}

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
        for (auto const& [key, value] : child_)
        {
            if (parent_.exists(key))
            {
                --n;
            }
        }
        return n;
    };

    reference get(std::size_t i) const override {
        if (i < child_.size())
        {
            return child_.get(i);
        }
        MRDOX_ASSERT(i < size());
        std::size_t pi = i - child_.size();
        size_t const n = parent_.size();
        for (std::size_t j = 0; j < n; ++j)
        {
            auto el = parent_.get(j);
            if (child_.exists(el.key))
            {
                ++pi;
            }
            else if (j == pi)
            {
                return el;
            }
        }
        MRDOX_UNREACHABLE();
    };

    dom::Value find(std::string_view key) const override {
        if (child_.exists(key))
        {
            return child_.find(key);
        }
        if (parent_.exists(key))
        {
            return parent_.find(key);
        }
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

dom::Object
createFrame(dom::Value const& parent)
{
    if (parent.isObject())
    {
        return createFrame(parent.getObject());
    }
    return {};
}

dom::Value
safeString(std::string_view str)
{
    dom::Value w(str);
    w.kind_ = dom::Kind::SafeString;
    return w;
}

dom::Value
safeString(dom::Value const& str)
{
    if (str.isString() || str.isSafeString())
    {
        return safeString(str.getString().get());
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
        case '=':
            out << "&#x3D;";
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
    struct RenderState
    {
        std::string_view templateText0;
        std::string_view templateText;
        std::vector<detail::partials_view_map> inlinePartials;
        std::vector<std::string_view> partialBlocks;
        std::size_t partialBlockLevel = 0;
        dom::Object data;
        dom::Object blockValues;
        dom::Object blockValuePaths;
        std::vector<dom::Value> parentContext;
        dom::Value rootContext;
        std::vector<dom::Object> dataStack;
    };
}

//std::string
//HandlebarsCallback::
//fn(dom::Value const& context,
//   dom::Object const& data,
//   dom::Array const& blockParams,
//   dom::Array const& blockParamPaths) const {
//    std::string result;
//    OutputRef out(result);
//    fn(out, context, data, blockParams, blockParamPaths);
//    return result;
//}
//
//void
//HandlebarsCallback::
//fn(OutputRef out,
//   dom::Value const& context,
//   dom::Object const& data,
//   dom::Array const& blockParams,
//   dom::Array const& blockParamPaths) const {
//    if (!isBlock()) {
//        return;
//    }
//    bool const sameContext =
//        &context == context_ ||
//        (context.isObject() && context_->isObject() && context.getObject().impl() == context_->getObject().impl()) ||
//        (context.isArray() && context_->isArray() && context.getArray().impl() == context_->getArray().impl());
//    if (!sameContext)
//    {
//        renderState_->parentContext.push_back(*context_);
//    }
//    dom::Object blockValues;
//    dom::Object blockValuePaths;
//    for (std::size_t i = 0; i < blockParamIds_.size(); ++i) {
//        blockValues.set(blockParamIds_[i], blockParams[i]);
//        if (blockParamPaths.size() > i) {
//            blockValuePaths.set(blockParamIds_[i], blockParamPaths[i]);
//        }
//    }
//    fn_(out, context, data, blockValues, blockValuePaths);
//    if (!sameContext)
//    {
//        renderState_->parentContext.pop_back();
//    }
//}
//
//std::string
//HandlebarsCallback::
//fn(dom::Value const& context) const {
//    return fn(context, data(), dom::Array{}, dom::Array{});
//}
//
//void
//HandlebarsCallback::
//fn(OutputRef out, dom::Value const& context) const {
//    fn(out, context, data(), dom::Array{}, dom::Array{});
//}
//
//std::string
//HandlebarsCallback::
//inverse(
//    dom::Value const& context,
//    dom::Object const& data,
//    dom::Array const& blockParams,
//    dom::Array const& blockParamPaths) const {
//    std::string result;
//    OutputRef out(result);
//    inverse(out, context, data, blockParams, blockParamPaths);
//    return result;
//}
//
//void
//HandlebarsCallback::
//inverse(OutputRef out,
//   dom::Value const& context,
//   dom::Object const& data,
//   dom::Array const& blockParams,
//   dom::Array const& blockParamPaths) const {
//    if (!isBlock()) {
//        return;
//    }
//    if (&context != context_)
//    {
//        renderState_->parentContext.push_back(*context_);
//    }
//    dom::Object blockValues;
//    dom::Object blockValuePaths;
//    for (std::size_t i = 0; i < blockParamIds_.size(); ++i) {
//        blockValues.set(blockParamIds_[i], blockParams[i]);
//        blockValuePaths.set(blockParamIds_[i], blockParamPaths[i]);
//    }
//    inverse_(out, context, data, blockValues, blockValuePaths);
//    if (&context != context_)
//    {
//        renderState_->parentContext.pop_back();
//    }
//}
//
//std::string
//HandlebarsCallback::
//inverse(dom::Value const& context) const {
//    return inverse(context, data(), dom::Array{}, dom::Array{});
//}
//
//void
//HandlebarsCallback::
//inverse(OutputRef out, dom::Value const& context) const {
//    inverse(out, context, data(), dom::Array{}, dom::Array{});
//}
//
//void
//HandlebarsCallback::
//log(dom::Value const& level,
//    dom::Array const& args) const {
//    MRDOX_ASSERT(logger_);
//    (*logger_)(level, args);
//}

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
Expected<void, HandlebarsError>
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
            auto res = find_position_in_text(state.templateText0, path0);
            if (res)
            {
                return Unexpected(
                    HandlebarsError(msg, res.line, res.column, res.pos));
            }
            return Unexpected(HandlebarsError(msg));
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
            std::string msg = fmt::format(
                "\"{}\" not defined in {}", literalSegment, toString(context));
            auto res = find_position_in_text(state.templateText0, literalSegment);
            if (res)
            {
                throw HandlebarsError(msg, res.line, res.column, res.pos);
            }
            throw HandlebarsError(msg);
        }
        else
        {
            return {dom::Kind::Undefined, false};
        }
    }
    else
    {
        cur = context.find(literalSegment);
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
                cur = obj.find(literalSegment);
            }
            else
            {
                if (opt.strict)
                {
                    std::string msg = fmt::format(
                        "\"{}\" not defined in {}", literalSegment, toString(cur));
                    auto res = find_position_in_text(state.templateText0, literalSegment);
                    if (res)
                    {
                        throw HandlebarsError(msg, res.line, res.column, res.pos);
                    }
                    throw HandlebarsError(msg);
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
Expected<std::pair<dom::Value, bool>, HandlebarsError>
lookupPropertyImpl(
    dom::Value const& context,
    std::string_view path,
    detail::RenderState const& state,
    HandlebarsOptions const& opt)
{
    using Res = std::pair<dom::Value, bool>;
    MRDOX_TRY(checkPath(path, state));

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
            std::string msg = fmt::format("\"{}\" not defined in {}", path, context);
            auto res = find_position_in_text(state.templateText0, path);
            if (res)
            {
                return Unexpected(HandlebarsError(msg, res.line, res.column, res.pos));
            }
            return Unexpected(HandlebarsError(msg));
        }
        return Res{nullptr, false};
    }
    // ==============================================================
    // Object path
    // ==============================================================
    return lookupPropertyImpl(context.getObject(), path, state, opt);
}

template <std::convertible_to<std::string_view> S>
static Expected<std::pair<dom::Value, bool>, HandlebarsError>
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
Expected<std::pair<dom::Value, bool>, HandlebarsError>
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
        dom::Value level = lookupLevel(args[0]);
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
        fmt::println("{}", out);
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
        MRDOX_ASSERT(t.buffer.data() >= context.data());
        MRDOX_ASSERT(t.buffer.data() + t.buffer.size() <= context.data() + context.size());

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

Expected<void, HandlebarsError>
Handlebars::
try_render_to(
    OutputRef& out,
    std::string_view templateText,
    dom::Value const& context,
    HandlebarsOptions const& options) const
{
    detail::RenderState state;
    state.templateText0 = templateText;
    state.templateText = templateText;
    if (options.data.isObject()) {
        state.data = options.data.getObject();
    }
    state.inlinePartials.emplace_back();
    state.rootContext = context;
    state.dataStack.emplace_back(state.data);
    return try_render_to_impl(out, context, options, state);
}

Expected<void, HandlebarsError>
Handlebars::
try_render_to_impl(
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
        MRDOX_TRY(renderTag(tag, out, context, opt, state));

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

std::string
appendContextPath(dom::Value const& contextPath, std::string_view id) {
    if (contextPath.isString()) {
        return std::string(contextPath.getString()) + "." + std::string(id);
    }
    else
    {
        return std::string(id);
    }
}

std::string
appendContextPath(dom::Value const& contextPath, dom::Value const& id) {
    if (!id.isString() && !id.isSafeString())
    {
        return std::string(contextPath);
    }
    return appendContextPath(contextPath, id.getString().get());
}
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
    type_key() const noexcept override {
        return "HandlebarsHelperObject";
    }

    std::size_t size() const override {
        return 13 + overlay_.size();
    }

    reference get(std::size_t i) const override {
        switch (i) {
        case 0: return { "name", name_ };
        case 1: return { "context", context_ };
        case 2: return { "data", data_ };
        case 3: return { "log", log_ };
        case 4: return { "hash", hash_ };
        case 5: return { "ids", ids_ };
        case 6: return { "hashIds", hashIds_ };
        case 7: return { "lookupProperty", lookupProperty_ };
        case 8: return { "blockParams", blockParams_ };
        case 9: return { "write", write_ };
        case 10: return { "fn", fn_ };
        case 11: return { "inverse", inverse_ };
        case 12: return { "write_inverse", write_inverse_ };
        default: return overlay_.get(i - 13);
        }
    }

    dom::Value find(std::string_view key) const override {
        if (key == "name")
        {
            return name_;
        }
        else if (key == "context")
        {
            return context_;
        }
        else if (key == "data")
        {
            return data_;
        }
        else if (key == "log")
        {
            return log_;
        }
        else if (key == "hash")
        {
            return hash_;
        }
        else if (key == "ids")
        {
            return ids_;
        }
        else if (key == "hashIds")
        {
            return hashIds_;
        }
        else if (key == "lookupProperty")
        {
            return lookupProperty_;
        }
        else if (key == "blockParams")
        {
            return blockParams_;
        }
        else if (key == "write")
        {
            return write_;
        }
        else if (key == "fn")
        {
            return fn_;
        }
        else if (key == "inverse")
        {
            return inverse_;
        }
        else if (key == "write_inverse")
        {
            return write_inverse_;
        }
        else
        {
            return overlay_.find(key);
        }
    }

    void set(dom::String key, dom::Value value) override {
        if (key == "name")
        {
            name_ = value;
        }
        else if (key == "context")
        {
            context_ = value;
        }
        else if (key == "data")
        {
            data_ = value;
        }
        else if (key == "log")
        {
            log_ = value;
        }
        else if (key == "hash")
        {
            hash_ = value;
        }
        else if (key == "ids")
        {
            ids_ = value;
        }
        else if (key == "hashIds")
        {
            hashIds_ = value;
        }
        else if (key == "lookupProperty")
        {
            lookupProperty_ = value;
        }
        else if (key == "blockParams")
        {
            blockParams_ = value;
        }
        else if (key == "write")
        {
            write_ = value;
        }
        else if (key == "fn")
        {
            fn_ = value;
        }
        else if (key == "inverse")
        {
            inverse_ = value;
        }
        else if (key == "write_inverse")
        {
            write_inverse_ = value;
        }
        else
        {
            overlay_.set(key, value);
        }
    }
};

Expected<Handlebars::evalExprResult, HandlebarsError>
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
                auto res = find_position_in_text(state.templateText0, helper);
                std::string msg(helper);
                msg += " is not a function";
                if (res)
                {
                    return Unexpected(HandlebarsError(msg, res.line, res.column, res.pos));
                }
                return Unexpected(HandlebarsError(msg));
            }
            all.remove_prefix(helper.data() + helper.size() - all.data());
            dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
            dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
            cb.set("name", helper);
            cb.set("context", context);
            setupArgs(all, context, state, args, cb, opt);
            return Res{fn.call(args).value(), true, false, true};
            MRDOX_UNREACHABLE();
        }
    }
    // ==============================================================
    // Private data
    // ==============================================================
    if (expression.starts_with('@'))
    {
        MRDOX_TRY(checkPath(expression, state));
        expression.remove_prefix(1);
        dom::Value data = state.data;
        if (expression == "root" || expression.starts_with("root.") || expression.starts_with("root/"))
        {
            popFirstSegment(expression);
            if (state.data.exists("root"))
            {
                data = state.data.find("root");
            }
            else
            {
                data = state.rootContext;
            }
        }
        else if (expression.starts_with("./") || expression.starts_with("../"))
        {
            auto rDataStack = std::ranges::views::reverse(state.dataStack);
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
        MRDOX_TRY(auto r, lookupPropertyImpl(data, expression, state, opt));
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
        MRDOX_TRY(auto r, lookupPropertyImpl(parentCtx, expression, state, noStrict));
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
        MRDOX_TRY(std::tie(r, defined), lookupPropertyImpl(context, expression, state, noStrict));
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
    // Whole context object key
    // ==============================================================
    if (context.kind() == dom::Kind::Object) {
        auto& obj = context.getObject();
        if (obj.exists(expression))
        {
            return Res{obj.find(expression), true, false};
        }
    }

    // ==============================================================
    // Context values
    // ==============================================================
    HandlebarsOptions strictOpt = opt;
    strictOpt.strict = opt.strict && !opt.compat;
    strictOpt.assumeObjects = opt.assumeObjects && !opt.compat;
    MRDOX_TRY(std::tie(r, defined), lookupPropertyImpl(context, expression, state, strictOpt));
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
        for (const auto& parentContext: parentContexts)
        {
            MRDOX_TRY(std::tie(r, defined), lookupPropertyImpl(parentContext, expression, state, noStrict));
            if (defined)
            {
                return Res{r, defined, false};
            }
        }
    }

    if (opt.strict)
    {
        std::string msg = fmt::format("\"{}\" not defined", expression);
        return Unexpected(HandlebarsError(msg));
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
Expected<void, HandlebarsError>
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

        Handlebars::Tag curTag = parseTag(tagStr, state.templateText0);

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
                        auto res = find_position_in_text(state.templateText0, blockName);
                        std::string msg(blockName);
                        msg += " doesn't match ";
                        msg += closeTag.content;
                        if (res)
                        {
                            return Unexpected(HandlebarsError(msg, res.line, res.column, res.pos));
                        }
                        return Unexpected(HandlebarsError(msg));
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
        auto res = find_position_in_text(state.templateText0, blockName);
        std::string msg(blockName);
        msg += " missing closing braces";
        if (res)
        {
            return Unexpected(HandlebarsError(msg, res.line, res.column, res.pos));
        }
        return Unexpected(HandlebarsError(msg));
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
        else if (templateText.starts_with("\r\n"))
        {
            templateText.remove_prefix(2);
        }
    }
    return {};
}


// Render a handlebars tag
Expected<void, HandlebarsError>
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

Expected<void, HandlebarsError>
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
    opt2.noEscape = tag.forceNoHTMLEscape || opt.noEscape;

    // ==============================================================
    // Helpers as block params
    // ==============================================================
    if (state.blockValues.exists(tag.helper))
    {
        auto v = state.blockValues.find(tag.helper);
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
        cb.set("data", state.data);
        cb.set("log", logger_);
        HandlebarsOptions noStrict = opt;
        noStrict.strict = false;
        MRDOX_TRY(setupArgs(tag.arguments, context, state, args, cb, noStrict));
        dom::Value res = fn.call(args).value();
        if (!res.isUndefined()) {
            opt2.noEscape = opt2.noEscape || res.isSafeString();
            format_to(out, res, opt2);
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
    MRDOX_TRY(auto resV, evalExpr(context, helper_expr, state, opt, false));
    if (resV.found)
    {
        if (resV.value.isFunction())
        {
            dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
            dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
            cb.set("name", helper_expr);
            cb.set("context", context);
            cb.set("data", state.data);
            cb.set("log", logger_);
            HandlebarsOptions noStrict = opt;
            noStrict.strict = false;
            setupArgs(tag.arguments, context, state, args, cb, noStrict);
            auto v2 = resV.value.getFunction().call(args).value();
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
        std::string msg = fmt::format("\"{}\" not defined in {}", helper_expr, toString(context));
        return Unexpected(HandlebarsError(msg));
    }

    // ==============================================================
    // helperMissing hook
    // ==============================================================
    auto [fn, found] = getHelper(helper_expr, false);
    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
    cb.set("name", helper_expr);
    cb.set("context", context);
    cb.set("data", state.data);
    cb.set("log", logger_);
    HandlebarsOptions noStrict = opt;
    noStrict.strict = false;
    setupArgs(tag.arguments, context, state, args, cb, noStrict);
    Expected<dom::Value> exp2 = fn.call(args);
    if (!exp2)
    {
        Error e = exp2.error();
        auto res = find_position_in_text(state.templateText0, helper_expr);
        std::string msg(e.reason());
        if (res)
        {
            return Unexpected(HandlebarsError(msg, res.line, res.column, res.pos));
        }
        return Unexpected(HandlebarsError(msg));
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

Expected<void, HandlebarsError>
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
    dom::Object hash = cb.find("hash").getObject();
    while (findExpr(expr, expression))
    {
        // ==========================================
        // Find next expression
        // ==========================================
        auto exprEndPos = expr.data() + expr.size() - expression.data();
        expression = expression.substr(exprEndPos);
        if (!expression.empty() && expression.front() != ' ')
        {
            std::string msg = fmt::format(
                "Parse error. Invalid helper expression. {}{}", expr, expression);
            auto res = find_position_in_text(expression, state.templateText0);
            if (res)
            {
                return Unexpected(HandlebarsError(msg, res.line, res.column, res.pos));
            }
            return Unexpected(HandlebarsError(msg));
        }
        expression = trim_ldelimiters(expression, " ");
        auto [k, v] = findKeyValuePair(expr);
        bool const isPositional = k.empty();
        if (isPositional)
        {
            // ==========================================
            // Positional argument
            // ==========================================
            MRDOX_TRY(auto res, evalExpr(context, expr, state, opt, true));
            args.emplace_back(res.value);
            if (opt.trackIds) {
                dom::Array ids = cb.find("ids").getArray();
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
                    std::size_t n = state.blockValuePaths.size();
                    dom::Value IdVal = expr;
                    for (std::size_t i = 0; i < n; ++i)
                    {
                        auto blockValuePath = state.blockValuePaths[i];
                        if (expr.starts_with(blockValuePath.key))
                        {
                            if (blockValuePath.value.isString())
                            {
                                std::string idStr;
                                idStr += blockValuePath.value.getString();
                                idStr += expr.substr(blockValuePath.key.size());
                                IdVal = idStr;
                            }
                            break;
                        }
                    }
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
            MRDOX_TRY(auto res, evalExpr(context, v, state, opt, true));
            hash.set(k, res.value);
            if (opt.trackIds) {
                dom::Object hashIds = cb.find("hashIds").getObject();
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
        dom::Value const& obj, dom::Value const& field)
    {
        return lookupPropertyImpl(obj, field, state, opt).value().first;
    }));
    args.emplace_back(cb);
    return {};
}

Expected<void, HandlebarsError>
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
        out << fmt::format(R"([undefined decorator "{}" in "{}"])", tag.helper, tag.buffer);
        return {};
    }

    // ==============================================================
    // Evaluate expression
    // ==============================================================
    std::string_view expr;
    findExpr(expr, tag.arguments);
    MRDOX_TRY(auto res, evalExpr(context, expr, state, opt, true));
    if (!res.value.isString())
    {
        out << fmt::format(R"([invalid decorator expression "{}" in "{}"])", tag.arguments, tag.buffer);
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

Expected<void, HandlebarsError>
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
        MRDOX_TRY(auto res, evalExpr(context, expr, state, opt, true));
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
            return Unexpected(HandlebarsError(fmt::format(
                "The partial {} could not be found", partialName)));
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
        MRDOX_TRY(this->try_render_to_impl(dumb, context, opt, state));
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

    // ==========================================
    // Populate with arguments
    // ==========================================
    bool partialCtxChanged = false;
    dom::Value prevContextPath = state.data.find("contextPath");
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
                    std::string msg = fmt::format(
                        "Unsupported number of partial arguments: {}", n);
                    auto res = find_position_in_text(state.templateText0, tag.buffer);
                    if (res)
                    {
                        return Unexpected(HandlebarsError(msg, res.line, res.column, res.pos));
                    }
                    return Unexpected(HandlebarsError(msg));
                }

                // Do change the context
                MRDOX_TRY(auto res, evalExpr(context, expr, state, opt, true));
                if (opt.trackIds)
                {
                    std::string contextPath = appendContextPath(
                        state.data.find("contextPath"), expr);
                    state.data.set("contextPath", contextPath);
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
                MRDOX_TRY(res, evalExpr(context, contextKey, state, opt, true));
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
                state.data.set("contextPath", true);
            }
        }
    }

    // ==============================================================
    // Render partial
    // ==============================================================
    // ==========================================
    // Setup partial state
    // ==========================================
    std::string_view templateText0 = state.templateText0;
    state.templateText0 = partial_content;
    std::string_view templateText = state.templateText;
    state.templateText = partial_content;
    bool const isPartialBlock = partialName == "@partial-block";
    state.partialBlockLevel -= isPartialBlock;
    out.setIndent(out.getIndent() + tag.standaloneIndent * !opt.preventIndent);
    if (partialCtxChanged)
    {
        state.parentContext.emplace_back(context);
    }
    state.dataStack.emplace_back(state.data);

    // ==========================================
    // Render partial
    // ==========================================
    MRDOX_TRY(this->try_render_to_impl(out, partialCtx, opt, state));

    // ==========================================
    // Restore state
    // ==========================================
    if (partialCtxChanged)
    {
        state.parentContext.pop_back();
    }
    state.dataStack.pop_back();
    out.setIndent(out.getIndent() - tag.standaloneIndent * !opt.preventIndent);
    state.partialBlockLevel += isPartialBlock;
    state.templateText = templateText;
    state.templateText0 = templateText0;
    if (opt.trackIds && partialCtxChanged)
    {
        state.data.set("contextPath", prevContextPath);
    }

    if (tag.type2 == '#')
    {
        state.inlinePartials.pop_back();
        state.partialBlocks.pop_back();
        --state.partialBlockLevel;
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

Expected<void, HandlebarsError>
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
    MRDOX_TRY(parseBlock(
        blockName, tag, opt, state, state.templateText, out,
        fnBlock, inverseBlock, inverseTag, isChainedBlock));

    // ==============================================================
    // Find helper
    // ==============================================================
    bool const isNoArgBlock = tag.arguments.empty();
    auto [fn, found] = getHelper(tag.helper, isNoArgBlock);
    bool const useContextFunction = !found && !tag.arguments.empty();
    if (useContextFunction)
    {
        MRDOX_TRY(auto res, evalExpr(context, tag.helper, state, opt, false));
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
        std::string msg = fmt::format(
            "\"{}\" not defined in {}", tag.helper, toString(context));
        auto res = find_position_in_text(state.templateText0, tag.helper);
        if (res)
        {
            return Unexpected(HandlebarsError(msg, res.line, res.column, res.pos));
        }
        return Unexpected(HandlebarsError(msg));
    }

    // ==============================================================
    // Setup helper context
    // ==============================================================
    dom::Array args = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object cb = dom::newObject<HbsHelperObjectImpl>();
    cb.set("name", tag.helper);
    cb.set("context", context);
    cb.set("data", state.data);
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
            dom::Value const& options) -> Expected<void, HandlebarsError>
    {
        // ==========================================
        // Setup new render state
        // ==========================================
        std::string_view templateText = state.templateText;
        state.templateText = fnBlock;
        dom::Object prevStateData = state.data;
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
                dom::Value dataV = optObj.find("data");
                if (dataV.isObject())
                {
                    state.data = dataV.getObject();
                }
            }

            // Block params
            if (optObj.exists("blockParams"))
            {
                dom::Value blockParamsV = optObj.find("blockParams");
                if (blockParamsV.isArray())
                {
                    dom::Object newBlockValues;
                    dom::Array const& blockParams = blockParamsV.getArray();
                    for (std::size_t i = 0; i < blockParamIds.size(); ++i) {
                        newBlockValues.set(blockParamIds[i], blockParams[i]);
                    }
                    dom::Object blockValuesOverlay =
                        createFrame(newBlockValues, state.blockValues);
                    state.blockValues = std::move(blockValuesOverlay);
                }
            }

            // Block param paths
            if (optObj.exists("blockParamPaths"))
            {
                dom::Value blockParamPathsV = optObj.find("blockParamPaths");
                if (blockParamPathsV.isArray())
                {
                    dom::Array const& blockParamPaths = blockParamPathsV.getArray();
                    dom::Object newBlockValuePaths;
                    for (std::size_t i = 0; i < blockParamIds.size(); ++i)
                    {
                        newBlockValuePaths.set(blockParamIds[i], blockParamPaths[i]);
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
        MRDOX_TRY(try_render_to_impl(out, newContext, opt, state));

        // ==========================================
        // Restore state
        // ==========================================
        state.templateText = templateText;
        state.data = std::move(prevStateData);
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
            dom::Value const& options) -> Expected<void, HandlebarsError>
    {
        // ==========================================
        // Setup new render state
        // ==========================================
        std::string_view templateText = state.templateText;
        state.templateText = inverseBlock;
        dom::Object prevStateData = state.data;
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
                dom::Value dataV = optObj.find("data");
                if (dataV.isObject())
                {
                    state.data = dataV.getObject();
                }
            }

            // Block params
            if (optObj.exists("blockParams"))
            {
                dom::Value blockParamsV = optObj.find("blockParams");
                if (blockParamsV.isArray())
                {
                    dom::Object newBlockValues;
                    dom::Array const& blockParams = blockParamsV.getArray();
                    for (std::size_t i = 0; i < blockParamIds.size(); ++i) {
                        newBlockValues.set(blockParamIds[i], blockParams[i]);
                    }
                    dom::Object blockValuesOverlay =
                        createFrame(newBlockValues, state.blockValues);
                    state.blockValues = std::move(blockValuesOverlay);
                }
            }

            // Block param paths
            if (optObj.exists("blockParamPaths"))
            {
                dom::Value blockParamPathsV = optObj.find("blockParamPaths");
                if (blockParamPathsV.isArray())
                {
                    dom::Array const& blockParamPaths = blockParamPathsV.getArray();
                    dom::Object newBlockValuePaths;
                    for (std::size_t i = 0; i < blockParamIds.size(); ++i)
                    {
                        newBlockValuePaths.set(blockParamIds[i], blockParamPaths[i]);
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
            MRDOX_TRY(try_render_to_impl(out, newContext, opt, state));
        }
        else
        {
            // Inverse tag contains its own helper
            // Go straight to the render block method, which will
            // expect a closing tag matching the parent tag, and
            // interpret sequential "else" tags as more chained
            // inverse tags.
            MRDOX_TRY(renderBlock(blockName, inverseTag, out, newContext, opt, state, true));
        }

        // ==========================================
        // Restore state
        // ==========================================
        state.templateText = templateText;
        state.data = std::move(prevStateData);
        state.blockValues = std::move(prevBlockValues);
        state.blockValuePaths = std::move(prevBlockValuePaths);
        if (!sameContext)
        {
            state.parentContext.pop_back();
        }
        return {};
    };

    std::optional<HandlebarsError> hbs_error;
    if (!tag.rawBlock) {
        cb.set("write", dom::makeInvocable([&out, &write_nested_block, &hbs_error](
            dom::Value const& newContext,
            dom::Value const& options) -> Expected<dom::Value> {
            auto exp = write_nested_block(out, newContext, options);
            if (!exp)
            {
                hbs_error = exp.error();
                return Unexpected(Error("Error in block helper"));
            }
            return dom::Value();
        }));
        cb.set("fn", dom::makeInvocable([&write_nested_block, &hbs_error](
            dom::Value const& newContext,
            dom::Value const& options) -> Expected<dom::Value> {
            std::string out;
            OutputRef out2(out);
            auto exp = write_nested_block(out2, newContext, options);
            if (!exp)
            {
                hbs_error = exp.error();
                return Unexpected(Error("Error in block helper"));
            }
            return out;
        }));
        cb.set("write_inverse", dom::makeInvocable([&out, &write_inverse_block, &hbs_error](
            dom::Value const& newContext,
            dom::Value const& options) -> Expected<dom::Value> {
            auto exp = write_inverse_block(out, newContext, options);
            if (!exp)
            {
                hbs_error = exp.error();
                return Unexpected(Error("Error in block helper"));
            }
            return dom::Value();
        }));
        cb.set("inverse", dom::makeInvocable([&write_inverse_block, &hbs_error](
            dom::Value const& newContext,
            dom::Value const& options) -> Expected<dom::Value> {
            std::string out;
            OutputRef out2(out);
            auto exp = write_inverse_block(out2, newContext, options);
            if (!exp)
            {
                hbs_error = exp.error();
                return Unexpected(Error("Error in block helper"));
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
        auto fnV = cb.find("fn");
        auto inverse = cb.find("inverse");
        cb.set("fn", inverse);
        cb.set("inverse", fnV);
        auto fn_write = cb.find("write");
        auto write_inverse = cb.find("write_inverse");
        cb.set("write", write_inverse);
        cb.set("write_inverse", fn_write);
    }

    // ==============================================================
    // Call helper
    // ==============================================================
    if (emulateMustache && args[0].isFunction())
    {
        // When emulating mustache, if the first argument
        // is a function, we call this function before
        // passing it to blockHelperMissing
        args.set(0, args[0](cb));
    }
    state.inlinePartials.emplace_back();
    // state.parentContext.emplace_back(context);
    state.dataStack.emplace_back(state.data);
    Expected<dom::Value> exp2 = fn.call(args);
    if (!exp2)
    {
        if (hbs_error)
        {
            // The error happened when the helper called fn or inverse,
            // and we have the exact HandlebarsError for that.
            return Unexpected(*hbs_error);
        }

        // The helper returned a regular Error, and we convert it to
        // a HandlebarsError with the same message and the position of
        // the helper call.
        Error e = exp2.error();
        auto res = find_position_in_text(state.templateText0, tag.buffer);
        if (res)
        {
            return Unexpected(HandlebarsError(e.reason(), res.line, res.column, res.pos));
        }
        return Unexpected(HandlebarsError(e.reason()));
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
    state.dataStack.pop_back();
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

namespace helpers {

Expected<void>
if_fn(dom::Array const& arguments)
{
    if (arguments.size() != 2)
    {
        return Unexpected(Error("#if requires exactly one argument"));
    }

    dom::Value conditional = arguments[0];
    dom::Value options = arguments[1];
    dom::Value context = options["context"];
    if (conditional.isFunction())
    {
        MRDOX_TRY(conditional, conditional.getFunction().try_invoke(context));
    }

    if ((!options["hash"]["includeZero"] && !conditional) || isEmpty(conditional)) {
        MRDOX_TRY(options["write_inverse"].getFunction().try_invoke(context));
    }
    else
    {
        MRDOX_TRY(options["write"].getFunction().try_invoke(context));
    }
    return {};
}

Expected<void>
unless_fn(dom::Array const& arguments)
{
    if (arguments.size() != 2) {
        return Unexpected(Error("#unless requires exactly one argument"));
    }

    dom::Value options = arguments[1];
    dom::Value fn = options["fn"];
    dom::Value inverse = options["inverse"];
    options.set("fn", inverse);
    options.set("inverse", fn);
    dom::Value write = options["write"];
    dom::Value write_inverse = options["write_inverse"];
    options.set("write", write_inverse);
    options.set("write_inverse", write);
    dom::Array inv_arguments = arguments;
    inv_arguments.set(1, options);
    return if_fn(inv_arguments);
}

Expected<void>
with_fn(dom::Array const& arguments)
{
    if (arguments.size() != 2) {
        return Unexpected(Error("#with requires exactly one argument"));
    }

    dom::Value context = arguments[0];
    dom::Value options = arguments[1];
    if (context.isFunction()) {
        MRDOX_TRY(context, context.getFunction().try_invoke(options["context"]));
    }

    if (!isEmpty(context))
    {
        dom::Value data = options["data"];
        if (data && options["ids"])
        {
            data = createFrame(data);
            data.set("contextPath", appendContextPath(
                data["contextPath"], options["ids"][0]));
        }
        dom::Array blockParams = {{context}};
        dom::Array blockParamPaths = {{data && data["contextPath"]}};
        dom::Object cbOpt;
        cbOpt.set("data", data);
        cbOpt.set("blockParams", blockParams);
        cbOpt.set("blockParamPaths", blockParamPaths);
        MRDOX_TRY(options["write"].getFunction().try_invoke(context, cbOpt));
    }
    else
    {
        MRDOX_TRY(options["write_inverse"].getFunction().try_invoke(options["context"]));
    }
    return {};
}

Expected<void>
each_fn(dom::Value context, dom::Value const& options)
{
    if (!options)
    {
        return Unexpected(Error("Must pass iterator to #each"));
    }

    dom::Value fn = options["write"];
    dom::Value inverse = options["write_inverse"];
    std::size_t i = 0;
    dom::Value data;
    std::string contextPath;

    if (options["data"] && options["ids"]) {
        contextPath = appendContextPath(
            options["data"]["contextPath"], options["ids"][0]) + '.';
    }

    if (context.isFunction()) {
        MRDOX_TRY(context, context.getFunction().try_invoke(options["context"]));
    }

    if (options["data"]) {
        data = createFrame(options["data"]);
    }

    auto execIteration = [&context, &data, &contextPath, &fn](
        dom::Value const& field, std::size_t index, dom::Value const& last)
        -> Expected<dom::Value>
    {
        if (data)
        {
            data.set("key", field);
            data.set("index", index);
            data.set("first", index == 0);
            data.set("last", last.getBool());
        }

        if (!contextPath.empty())
        {
            data.set("contextPath", contextPath + field);
        }

        dom::Array blockParams = {{context[field], field}};
        dom::Array blockParamPaths = {{data && data["contextPath"], nullptr}};
        dom::Object cbOpt;
        cbOpt.set("data", data);
        cbOpt.set("blockParams", blockParams);
        cbOpt.set("blockParamPaths", blockParamPaths);
        return fn.getFunction().try_invoke(context[field], cbOpt);
    };

    bool const isJSObject = static_cast<bool>(
        context && (context.isObject() || context.isArray()));
    if (isJSObject)
    {
        if (context.isArray())
        {
            std::size_t const n = context.size();
            for (; i < n; ++i)
            {
                bool const isLast = i == n - 1;
                MRDOX_TRY(execIteration(i, static_cast<std::int64_t>(i), isLast));
            }
        }
        else if (context.isObject())
        {
            dom::Value priorKey;
            for (auto const& [key, value] : context.getObject())
            {
                if (!priorKey.isUndefined())
                {
                    MRDOX_TRY(execIteration(priorKey, i - 1, false));
                }
                priorKey = key;
                ++i;
            }
            if (!priorKey.isUndefined())
            {
                MRDOX_TRY(execIteration(priorKey, i - 1, true));
            }
        }
    }

    if (i == 0)
    {
        MRDOX_TRY(inverse.getFunction().try_invoke(options["context"]));
    }
    return {};
}

Expected<dom::Value>
lookup_fn(dom::Value const& obj, dom::Value const& field, dom::Value const& options)
{
    if (!obj)
    {
        return obj;
    }
    return options["lookupProperty"].getFunction().try_invoke(obj, field);
}

Expected<void>
log_fn(dom::Array const& arguments)
{
    // {level, args}
    dom::Array args = {{dom::Value{}}};
    dom::Value options = arguments.back();
    std::size_t const n = arguments.size();
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        args.emplace_back(arguments[i]);
    }

    dom::Value level = 1;
    dom::Value hash = options["hash"];
    dom::Value data = options["data"];
    if (hash.exists("level") && !hash["level"].isNull())
    {
        level = options["hash"]["level"];
    }
    else if (data.exists("level") && !data["level"].isNull())
    {
        level = options["data"]["level"];
    }
    args.set(0, level);
    MRDOX_TRY(options["log"].getFunction().call(args));
    return {};
}

Expected<dom::Value>
helper_missing_fn(dom::Array const& arguments)
{
    if (arguments.size() == 1)
    {
        return {};
    }

    return Unexpected(Error(fmt::format(
        R"(Missing helper: "{}")",
        arguments.back()["name"])));
}

Expected<void>
block_helper_missing_fn(
    dom::Value const& context, dom::Value options)
{
    if (context == true)
    {
        MRDOX_TRY(options["write"].getFunction().try_invoke(options["context"]));
    }
    else if (
        context == false ||
        context.isNull() ||
        context.isUndefined())
    {
        MRDOX_TRY(options["write_inverse"].getFunction().try_invoke(options["context"]));
    }
    else if (context.isArray())
    {
        if (!context.empty())
        {
            if (options["ids"])
            {
                options.set("ids", dom::Array{{options["name"]}});
            }
            MRDOX_TRY(each_fn(context, options));
        }
        else
        {
            MRDOX_TRY(options["write_inverse"].getFunction().try_invoke(options["context"]));
        }
    }
    else
    {
        dom::Object fnOpt;
        if (options["data"] && options["ids"])
        {
            dom::Object data = createFrame(options["data"]);
            data.set(
                "contextPath",
                appendContextPath(data["contextPath"], options["name"]));
            fnOpt.set("data", data);
        }
        MRDOX_TRY(options["write"].getFunction().try_invoke(context, fnOpt));
    }
    return {};
}

void
registerBuiltinHelpers(Handlebars& hbs)
{
    hbs.registerHelper("if", dom::makeVariadicInvocable(if_fn));
    hbs.registerHelper("unless", dom::makeVariadicInvocable(unless_fn));
    hbs.registerHelper("with", dom::makeVariadicInvocable(with_fn));
    hbs.registerHelper("each", dom::makeInvocable(each_fn));
    hbs.registerHelper("lookup", dom::makeInvocable(lookup_fn));
    hbs.registerHelper("log", dom::makeVariadicInvocable(log_fn));
    hbs.registerHelper("helperMissing", dom::makeVariadicInvocable(helper_missing_fn));
    hbs.registerHelper("blockHelperMissing", dom::makeInvocable(block_helper_missing_fn));
}

void
registerAntoraHelpers(Handlebars& hbs)
{
    hbs.registerHelper("and", dom::makeVariadicInvocable(and_fn));
    hbs.registerHelper("detag", dom::makeInvocable(detag_fn));
    hbs.registerHelper("eq", dom::makeVariadicInvocable(eq_fn));
    hbs.registerHelper("increment", dom::makeInvocable(increment_fn));
    hbs.registerHelper("ne", dom::makeVariadicInvocable(ne_fn));
    hbs.registerHelper("not", dom::makeVariadicInvocable(not_fn));
    hbs.registerHelper("or", dom::makeVariadicInvocable(or_fn));
    hbs.registerHelper("relativize", dom::makeInvocable(relativize_fn));
    hbs.registerHelper("year", dom::makeInvocable(year_fn));
}

bool
and_fn(dom::Array const& args)
{
    std::size_t const n = args.size();
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        if (!args[i])
        {
            return false;
        }
    }
    return true;
}

bool
or_fn(dom::Array const& args) {
    std::size_t const n = args.size();
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        if (args[i])
        {
            return true;
        }
    }
    return false;
}

bool
eq_fn(dom::Array const& args) {
    if (args.empty())
    {
        return true;
    }
    dom::Value first = args[0];
    std::size_t const n = args.size();
    for (std::size_t i = 1; i < n - 1; ++i)
    {
        if (first != args[i])
        {
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

bool
not_fn(dom::Array const& args) {
    std::size_t const n = args.size();
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        if (!args[i])
        {
            return true;
        }
    }
    return false;
}

dom::Value
increment_fn(dom::Value const& value)
{
    if (value)
    {
        return value + 1;
    }
    return 1;
}

dom::Value
detag_fn(dom::Value html)
{
    if (!html)
    {
        return html;
    }

    // remove instances of "/<[^>]+>/g"
    std::string result;
    result.reserve(html.size());
    bool insideTag = false;
    for (char c: html.getString().get())
    {
        if (c == '<')
        {
            insideTag = true;
            continue;
        }
        if (c == '>')
        {
            insideTag = false;
            continue;
        }
        if (!insideTag)
        {
            result.push_back(c);
        }
    }
    return result;
}

dom::Value
relativize_fn(dom::Value to, dom::Value from, dom::Value context)
{
    // https://gitlab.com/antora/antora-ui-default/-/blob/master/src/helpers/relativize.js
    if (!to)
    {
        return "#";
    }

    if (to.isString() && !std::string_view(to.getString()).starts_with('/'))
    {
        return to;
    }

    if (!context)
    {
        // NOTE only legacy invocation provides both to and from
        context = from;
        from = context["data"]["root"]["page"]["url"];
    }

    if (!from)
    {
        dom::Value sitePath = context["data"]["root"]["site"]["path"];
        if (sitePath)
        {
            return sitePath + to;
        }
        return to;
    }

    dom::Value hash = "";
    std::size_t hashIdx = to.getString().get().find('#');
    if (hashIdx != std::string_view::npos)
    {
        hash = to.getString().get().substr(hashIdx);
        to = to.getString().get().substr(0, hashIdx);
    }

    /// return to === from
    //    ? hash || (isDir(to) ? './' : path.basename(to))
    //    : (path.relative(path.dirname(from + '.'), to) || '.') + (isDir(to) ? '/' + hash : hash)
    if (to == from)
    {
        if (hash) {
            return hash;
        }
        else if (to.isString() && files::isDirsy(to.getString().get()))
        {
            return "./";
        }
        else if (to.isString())
        {
            return files::getFileName(to.getString().get());
        }
        else
        {
            return to;
        }
    }
    else
    {
        // AFREITAS: Implement this functionality without std::filesystem
        if (!to.isString() || !from.isString()) {
            return to;
        }
        std::string relativePath = std::filesystem::relative(
            std::filesystem::path(std::string_view(to.getString())),
            std::filesystem::path(std::string_view(from.getString()))).generic_string();
        if (relativePath.empty())
        {
            relativePath = ".";
        }
        if (files::isDirsy(to.getString())) {
            return relativePath + '/' + hash;
        }
        else
        {
            return relativePath + hash;
        }
    }
}

int
year_fn() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&currentTime);
    return localTime->tm_year + 1900;
}

constexpr
std::int64_t
normalize_index(std::int64_t i, std::int64_t n) {
    if (i < 0 || i > n)
    {
        return (i % n + n) % n;
    }
    return i;
}

dom::Value
at_fn(dom::Value range, dom::Value field, dom::Value options)
{
    auto isBlock = options.isUndefined() && static_cast<bool>(field["fn"]);
    if (isBlock)
    {
        options = field;
        field = range;
        range = options["fn"]();
    }

    std::int64_t index = 0;
    if (field.isInteger())
    {
        index = field.getInteger();
    }

    if (range.isString())
    {
        std::string str(range.getString().get());
        index = normalize_index(index, static_cast<std::int64_t>(str.size()));
        return std::string(1, str.at(index));
    }
    else if (range.isArray())
    {
        dom::Array const& arr = range.getArray();
        index = normalize_index(index, static_cast<std::int64_t>(arr.size()));
        return arr.at(index);
    }
    else if (range.isObject())
    {
        dom::Object const& obj = range.getObject();
        if (!field.isString())
        {
            return nullptr;
        }
        std::string key(field.getString().get());
        if (obj.exists(key))
        {
            return obj.find(key);
        }
        return nullptr;
    }
    else
    {
        return range;
    }
}

dom::Value
concat_fn(
    dom::Value range1,
    dom::Value sep,
    dom::Value range2,
    dom::Value options)
{
    auto isBlock = options.isUndefined() && static_cast<bool>(range2["fn"]);
    if (isBlock)
    {
        options = range2;
        range2 = sep;
        sep = range1;
        range1 = options["fn"]();
    }

    if (range1.isString() || range2.isString())
    {
        return range1 + sep + range2;
    }
    else if (range1.isArray() && sep.isArray())
    {
        options = range2;
        range2 = sep;
        dom::Array res;
        for (dom::Value item : range1.getArray())
        {
            res.emplace_back(item);
        }
        for (dom::Value item : range2.getArray())
        {
            res.emplace_back(item);
        }
        return res;
    }
    else if (range1.isObject() && sep.isObject())
    {
        options = range2;
        range2 = sep;
        return createFrame(range1.getObject(), range2.getObject());
    }
    return range1 + range2;
}

std::int64_t
count_fn(dom::Array const& arguments)
{
    std::size_t const n = arguments.size();
    dom::Value options = arguments.back();
    dom::Value fn = options["fn"];
    auto const isBlock = static_cast<bool>(fn);
    dom::Value firstArg = arguments[0];
    dom::Value secondArg = arguments[1];
    bool const stringOverload =
        (isBlock && firstArg.isString()) ||
        (firstArg.isString() && secondArg.isString());
    if (stringOverload)
    {
        std::string str;
        std::string sub;
        std::int64_t start = 0;
        std::int64_t end = 0;
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            sub = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = arguments[1].getInteger();
                if (n > 3)
                {
                    end = arguments[2].getInteger();
                }
            }
        }
        else /* !isBlock */
        {
            str = arguments[0].getString();
            sub = arguments[1].getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments[2].getInteger();
                if (n > 4)
                {
                    end = arguments[3].getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::int64_t count = 0;
        for (std::int64_t pos = start; pos < end; ++pos)
        {
            if (str.compare(pos, sub.size(), sub) == 0)
            {
                ++count;
            }
        }
        return count;
    }
    else
    {
        // Generic range overload
        dom::Value range = arguments[0];
        dom::Value item = arguments[1];
        if (range.isString())
        {
            // String overload: find chars in it
            std::string str(range.getString().get());
            char x = item.getString().get().at(0);
            return std::ranges::count(str, x);
        }
        else if (range.isArray())
        {
            // Array overload: find items in it
            dom::Array const& arr = range.getArray();
            return std::ranges::count(arr, item);
        }
        else if (range.isObject())
        {
            // Object overload: find values in it
            dom::Object const& obj = range.getObject();
            std::int64_t count = 0;
            for (auto const& [key, val] : obj)
            {
                if (val == item)
                {
                    ++count;
                }
            }
            return count;
        }
        else
        {
            // Generic overload: return 0
            return 0;
        }
    }
}

dom::Value
replace_fn(dom::Array const& arguments)
{
    std::size_t const n = arguments.size();
    dom::Value options = arguments.back();
    dom::Value fn = options["fn"];
    dom::Value firstArg = arguments[0];
    dom::Value secondArg = arguments[1];
    bool const isBlock = static_cast<bool>(fn);
    bool const stringOverload =
        (isBlock && firstArg.isString()) ||
        (firstArg.isString() && secondArg.isString());
    if (stringOverload)
    {
        // String overload: replace substrings
        std::string str;
        std::string old;
        std::string new_str;
        std::int64_t count = -1;
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            old = firstArg.getString();
            new_str = secondArg.getString();
            if (n > 3)
            {
                count = arguments.at(2).getInteger();
            }
        }
        else
        {
            str = firstArg.getString();
            old = secondArg.getString();
            new_str = arguments.at(2).getString();
            if (n > 4)
            {
                count = arguments.at(3).getInteger();
            }
        }
        std::string res;
        std::size_t pos = 0;
        std::size_t old_len = old.size();
        while (count != 0)
        {
            std::size_t next = str.find(old, pos);
            if (next == std::string::npos)
            {
                res += str.substr(pos);
                break;
            }
            res += str.substr(pos, next - pos);
            res += new_str;
            pos = next + old_len;
            if (count > 0)
            {
                --count;
            }
        }
        return res;
    }
    // Generic range overload
    dom::Value range = firstArg;
    dom::Value const& item = secondArg;
    dom::Value replacement = arguments.at(2);
    if (range.isString())
    {
        // String overload: replace chars in it
        std::string str(range.getString().get());
        auto str_n = static_cast<std::int64_t>(str.size());
        for (std::int64_t i = 0; i < str_n; ++i)
        {
            if (str.at(i) == item.getString().get().at(0))
            {
                str.replace(
                    str.begin() + i,
                    str.begin() + i + 1,
                    replacement.getString().get());
                std::size_t n2 = replacement.getString().get().size();
                i += static_cast<std::int64_t>(n2) - 1;
            }
        }
        return str;
    }
    else if (range.isArray())
    {
        // Array overload: replace items in it
        dom::Array const& arr = range.getArray();
        dom::Array res;
        std::size_t arr_n = arr.size();
        for (std::size_t i = 0; i < arr_n; ++i)
        {
            dom::Value v = arr.at(i);
            if (v == item)
            {
                res.emplace_back(replacement);
            }
            else
            {
                res.emplace_back(v);
            }
        }
        return res;
    }
    else if (range.isObject())
    {
        // Object overload: replace values in it
        dom::Object obj = createFrame(range.getObject());
        for (auto const& [key, val] : obj)
        {
            if (val == item)
            {
                obj.set(key, replacement);
            }
        }
        return obj;
    }
    else
    {
        // Generic overload: replace nothing
        return range;
    }
}

void
registerStringHelpers(Handlebars& hbs)
{
    static constexpr auto toupper = [](char c) -> char {
        return static_cast<char>(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
    };

    static constexpr auto toLower = [](char c) -> char {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    };

    hbs.registerHelper("to_json", [](dom::Value const& v) -> dom::Value {
        return dom::JSON::stringify(v);
    });

    hbs.registerHelper("capitalize", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        if (!res.empty())
        {
            res[0] = toupper(res[0]);
        }
        return res;
    }));

    hbs.registerHelper("center", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t width = 0;
        char fillchar = ' ';
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            width = firstArg.getInteger();
            if (n > 2)
            {
                fillchar = secondArg.getString().get()[0];
            }
        }
        else
        {
            res = firstArg.getString();
            width = secondArg.getInteger();
            if (n > 3)
            {
                fillchar = arguments.at(2).getString().get()[0];
            }
        }
        if (width > static_cast<std::int64_t>(res.size()))
        {
            std::size_t pad = (width - res.size()) / 2;
            res.insert(0, pad, fillchar);
            res.append(pad, fillchar);
        }
        return res;
    }));

    constexpr auto ljust_fn = [](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t width = 0;
        std::string fill = " ";
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            width = firstArg.getInteger();
            if (n > 2)
            {
                fill = secondArg.getString();
            }
        }
        else
        {
            res = firstArg.getString();
            width = secondArg.getInteger();
            if (n > 3)
            {
                fill = arguments.at(2).getString();
            }
        }
        auto text_size = static_cast<std::int64_t>(res.size());
        while (text_size < width)
        {
            auto const filled_size = text_size + static_cast<std::int64_t>(fill.size());
            if (filled_size > width)
            {
                res.append(fill, 0, width - res.size());
            }
            else
            {
                res.append(fill);
            }
            text_size = static_cast<std::int64_t>(res.size());
        }
        return res;
    };

    hbs.registerHelper("ljust", dom::makeVariadicInvocable(ljust_fn));
    hbs.registerHelper("pad_end", dom::makeVariadicInvocable(ljust_fn));

    static constexpr auto rjust_fn = [](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t width = 0;
        std::string fill = " ";
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            width = firstArg.getInteger();
            if (n > 2)
            {
                fill = secondArg.getString();
            }
        }
        else
        {
            res = firstArg.getString();
            width = secondArg.getInteger();
            if (n > 3)
            {
                fill = arguments.at(2).getString();
            }
        }
        auto text_size = static_cast<std::int64_t>(res.size());
        while (text_size < width) {
            auto filled_size = static_cast<std::int64_t>(
                res.size() + fill.size());
            if (filled_size > width)
            {
                res.insert(0, fill, 0, width - res.size());
            }
            else
            {
                res.insert(0, fill);
            }
            text_size = static_cast<std::int64_t>(res.size());
        }
        return res;
    };

    hbs.registerHelper("rjust", dom::makeVariadicInvocable(rjust_fn));
    hbs.registerHelper("pad_start", dom::makeVariadicInvocable(rjust_fn));

    hbs.registerHelper("count", dom::makeVariadicInvocable(count_fn));

    hbs.registerHelper("ends_with", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string suffix;
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            suffix = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = secondArg.getInteger();
                if (n > 3)
                {
                    end = arguments.at(2).getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            suffix = secondArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments.at(2).getInteger();
                if (n > 4)
                {
                    end = arguments.at(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::string substr = str.substr(start, end - start);
        return substr.ends_with(suffix);
    }));

    hbs.registerHelper("starts_with", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string prefix;
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        auto const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            prefix = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = secondArg.getInteger();
                if (n > 3)
                {
                    end = arguments.at(2).getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            prefix = secondArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments.at(2).getInteger();
                if (n > 4)
                {
                    end = arguments.at(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::string substr = str.substr(start, end - start);
        return substr.starts_with(prefix);
    }));

    hbs.registerHelper("expandtabs", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::int64_t tabsize = 8;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                tabsize = firstArg.getInteger();
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
            {
                tabsize = secondArg.getInteger();
            }
        }
        std::string res;
        res.reserve(str.size());
        for (char c : str)
        {
            if (c == '\t')
            {
                res.append(tabsize, ' ');
            }
            else
            {
                res.push_back(c);
            }
        }
        return res;
    }));

    static constexpr auto find_index_fn = [](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sub;
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            sub = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = secondArg.getInteger();
                if (n > 3)
                {
                    end = arguments.at(2).getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            sub = secondArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments.at(2).getInteger();
                if (n > 4)
                {
                    end = arguments.at(3).getInteger();
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

    hbs.registerHelper("find", dom::makeVariadicInvocable(find_index_fn));
    hbs.registerHelper("index_of", dom::makeVariadicInvocable(find_index_fn));

    hbs.registerHelper("includes", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        return find_index_fn(arguments) != -1;
    }));

    auto rfind_index_fn = [](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sub;
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            sub = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = secondArg.getInteger();
                if (n > 3)
                {
                    end = arguments.at(2).getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            sub = secondArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments.at(2).getInteger();
                if (n > 4)
                {
                    end = arguments.at(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::size_t pos = str.rfind(sub, start);
        bool const notFound = pos == std::string::npos ||
                              static_cast<std::int64_t>(pos) >= end;
        if (notFound)
        {
            return static_cast<std::int64_t>(-1);
        }
        return static_cast<std::int64_t>(pos);
    };

    hbs.registerHelper("rfind", dom::makeVariadicInvocable(rfind_index_fn));
    hbs.registerHelper("rindex_of", dom::makeVariadicInvocable(rfind_index_fn));
    hbs.registerHelper("last_index_of", dom::makeVariadicInvocable(rfind_index_fn));

    hbs.registerHelper("at", dom::makeInvocable(at_fn));
    hbs.registerHelper("char_at", dom::makeInvocable(at_fn));

    hbs.registerHelper("is_alnum", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return (c >= '0' && c <= '9') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z');
        });
    }));

    hbs.registerHelper("is_alpha", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z');
        });
    }));

    hbs.registerHelper("is_ascii", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return std::ranges::all_of(res, [](char c) {
            auto uc = static_cast<unsigned char>(c);
            return uc <= 127;
        });
    }));

    auto is_digits_fn = dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c >= '0' && c <= '9';
        });
    });

    hbs.registerHelper("is_decimal", is_digits_fn);
    hbs.registerHelper("is_digit", is_digits_fn);

    hbs.registerHelper("is_lower",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c >= 'a' && c <= 'z';
        });
    }));

    hbs.registerHelper("is_upper",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c >= 'A' && c <= 'Z';
        });
    }));

    hbs.registerHelper("is_printable",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c >= 32 && c <= 126;
        });
    }));

    hbs.registerHelper("is_space",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return std::ranges::all_of(res, [](char c) {
            return c == ' ' || (c >= 9 && c <= 13);
        });
    }));

    hbs.registerHelper("is_title",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
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
            }
            else
            {
                prev_is_cased = false;
            }
        }
        return res_is_title;
    }));

    static auto to_upper_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        std::ranges::transform(res, res.begin(), toupper);
        return res;
    });

    hbs.registerHelper("upper", to_upper_fn);
    hbs.registerHelper("to_upper", to_upper_fn);

    static auto to_lower_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        std::ranges::transform(res, res.begin(), toLower);
        return res;
    });

    hbs.registerHelper("lower", to_lower_fn);
    hbs.registerHelper("to_lower", to_lower_fn);

    hbs.registerHelper("swap_case", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        if (res.empty()) {
            return res;
        }
        std::ranges::transform(res, res.begin(), [](char c) -> char
        {
            if (c >= 'A' && c <= 'Z')
            {
                return toLower(c);
            }
            else if (c >= 'a' && c <= 'z')
            {
                return toupper(c);
            }
            else
            {
                return c;
            }
        });
        return res;
    }));

    static auto join_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        dom::Array arr;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            arr = firstArg.getArray();
        }
        else
        {
            str = firstArg.getString();
            arr = secondArg.getArray();
        }
        std::string res;
        std::size_t const n = arr.size();
        for (std::size_t i = 0; i < n; ++i)
        {
            if (!res.empty())
            {
                res += str;
            }
            auto const& item = arr.at(i);
            res += item.getString();
        }
        return res;
    });

    hbs.registerHelper("join", join_fn);
    hbs.registerHelper("implode", join_fn);

    hbs.registerHelper("concat", dom::makeInvocable(concat_fn));

    static auto strip_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string chars = " \t\r\n";
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                chars = firstArg.getString();
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
                chars = secondArg.getString();
        }
        std::size_t pos = str.find_first_not_of(chars);
        if (pos == std::string::npos)
        {
            return std::string();
        }
        std::size_t endpos = str.find_last_not_of(chars);
        return str.substr(pos, endpos - pos + 1);
    });

    hbs.registerHelper("strip", strip_fn);
    hbs.registerHelper("trim", strip_fn);

    static auto lstrip_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string chars = " \t\r\n";
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                chars = firstArg.getString();
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
            {
                chars = secondArg.getString();
            }
        }
        std::size_t pos = str.find_first_not_of(chars);
        if (pos == std::string::npos)
        {
            return std::string();
        }
        return str.substr(pos);
    });

    hbs.registerHelper("lstrip", lstrip_fn);
    hbs.registerHelper("trim_start", lstrip_fn);

    static auto rstrip_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string chars = " \t\r\n";
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                chars = firstArg.getString();
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
            {
                chars = secondArg.getString();
            }
        }
        std::size_t pos = str.find_last_not_of(chars);
        if (pos == std::string::npos) {
            return std::string();
        }
        return str.substr(0, pos + 1);
    });

    hbs.registerHelper("rstrip", rstrip_fn);
    hbs.registerHelper("trim_end", rstrip_fn);

    hbs.registerHelper("partition", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sep;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            sep = firstArg.getString();
        }
        else
        {
            str = firstArg.getString();
            sep = secondArg.getString();
        }
        dom::Array res;
        std::size_t pos = str.find(sep);
        if (pos == std::string::npos)
        {
            res.emplace_back(str);
            res.emplace_back(std::string());
            res.emplace_back(std::string());
        }
        else
        {
            res.emplace_back(str.substr(0, pos));
            res.emplace_back(sep);
            res.emplace_back(str.substr(pos + sep.size()));
        }
        return res;
    }));

    hbs.registerHelper("rpartition", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sep;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            sep = firstArg.getString();
        }
        else
        {
            str = firstArg.getString();
            sep = secondArg.getString();
        }
        dom::Array res;
        std::size_t pos = str.rfind(sep);
        if (pos == std::string::npos)
        {
            res.emplace_back(str);
            res.emplace_back(std::string());
            res.emplace_back(std::string());
        }
        else
        {
            res.emplace_back(str.substr(0, pos));
            res.emplace_back(sep);
            res.emplace_back(str.substr(pos + sep.size()));
        }
        return res;
    }));

    hbs.registerHelper("remove_prefix", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string prefix;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            prefix = firstArg.getString();
        }
        else
        {
            str = firstArg.getString();
            prefix = secondArg.getString();
        }
        if (str.size() < prefix.size())
        {
            return str;
        }
        if (str.substr(0, prefix.size()) != prefix)
        {
            return str;
        }
        return str.substr(prefix.size());
    }));

    hbs.registerHelper("remove_suffix", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string suffix;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            suffix = firstArg.getString();
        }
        else
        {
            str = firstArg.getString();
            suffix = secondArg.getString();
        }
        if (str.size() < suffix.size())
        {
            return str;
        }
        if (str.substr(str.size() - suffix.size()) != suffix)
        {
            return str;
        }
        return str.substr(0, str.size() - suffix.size());
    }));

    hbs.registerHelper("replace", dom::makeVariadicInvocable(replace_fn));

    static auto split_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sep = " ";
        std::int64_t maxsplit = -1;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1) {
                sep = firstArg.getString();
                if (n > 2)
                {
                    maxsplit = secondArg.getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
            {
                sep = secondArg.getString();
                if (n > 3)
                {
                    maxsplit = arguments.at(2).getInteger();
                }
            }
        }
        dom::Array res;
        std::size_t pos = 0;
        std::size_t sep_len = sep.size();
        while (maxsplit != 0)
        {
            std::size_t next = str.find(sep, pos);
            if (next == std::string::npos)
            {
                res.emplace_back(str.substr(pos));
                break;
            }
            res.emplace_back(str.substr(pos, next - pos));
            pos = next + sep_len;
            if (maxsplit > 0)
            {
                --maxsplit;
            }
        }
        return res;
    });

    hbs.registerHelper("split", split_fn);
    hbs.registerHelper("explode", split_fn);

    hbs.registerHelper("rsplit", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sep = " ";
        std::int64_t maxsplit = -1;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments[0];
        dom::Value secondArg = arguments[1];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                sep = firstArg.getString();
                if (n > 2)
                {
                    maxsplit = secondArg.getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2) {
                sep = secondArg.getString();
                if (n > 3)
                {
                    maxsplit = arguments.at(2).getInteger();
                }
            }
        }
        dom::Array res;
        std::size_t pos = str.size();
        std::size_t sep_len = sep.size();
        while (maxsplit != 0)
        {
            std::size_t next = str.rfind(sep, pos);
            if (next == std::string::npos)
            {
                res.emplace_back(str.substr(0, pos));
                break;
            }
            res.emplace_back(str.substr(next + sep_len, pos - next - sep_len));
            if (next == 0)
            {
                break;
            }
            pos = next - 1;
            if (maxsplit > 0)
            {
                --maxsplit;
            }
        }
        return res;
    }));

    hbs.registerHelper("split_lines", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        bool keepends = false;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options["fn"];
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                keepends = firstArg.getBool();
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
            {
                keepends = secondArg.getBool();
            }
        }
        dom::Array res;
        std::size_t pos = 0;
        std::size_t len = str.size();
        while (pos < len)
        {
            std::size_t next = str.find_first_of("\r\n", pos);
            if (next == std::string::npos)
            {
                res.emplace_back(str.substr(pos));
                break;
            }
            if (keepends)
            {
                res.emplace_back(str.substr(pos, next - pos + 1));
            }
            else
            {
                res.emplace_back(str.substr(pos, next - pos));
            }
            pos = next + 1;
        }
        return res;
    }));

    hbs.registerHelper("zfill", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t width = 0;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            width = firstArg.getInteger();
        }
        else
        {
            res = firstArg.getString();
            width = secondArg.getInteger();
        }
        if (width <= static_cast<std::int64_t>(res.size()))
        {
            return res;
        }
        std::string prefix;
        if (res[0] == '+' || res[0] == '-')
        {
            prefix = res[0];
            res = res.substr(1);
            if (width != static_cast<std::int64_t>(res.size()))
                --width;
        }
        std::string padding(width - res.size(), '0');
        return prefix + padding + res;
    }));

    hbs.registerHelper("repeat", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t count = 0;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            count = firstArg.getInteger();
        }
        else
        {
            res = firstArg.getString();
            count = secondArg.getInteger();
        }
        if (count <= 0)
        {
            return std::string();
        }
        std::string tmp;
        while (count > 0)
        {
            tmp += res;
            --count;
        }
        return tmp;
    }));

    hbs.registerHelper("escape", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        return escapeExpression(res);
    }));

    static auto slice_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t start = 0;
        std::int64_t stop = 0;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            stop = static_cast<std::int64_t>(res.size());
            start = firstArg.getInteger();
            if (n > 2)
            {
                stop = secondArg.getInteger();
            }
        }
        else
        {
            res = firstArg.getString();
            stop = static_cast<std::int64_t>(res.size());
            start = secondArg.getInteger();
            if (n > 3)
            {
                stop = arguments.at(2).getInteger();
            }
        }
        if (res.empty())
        {
            return std::string();
        }
        start = normalize_index(start, static_cast<std::int64_t>(res.size()));
        stop = normalize_index(stop, static_cast<std::int64_t>(res.size()));
        if (start >= stop)
        {
            return std::string();
        }
        return res.substr(start, stop - start);
    });

    hbs.registerHelper("slice", slice_fn);
    hbs.registerHelper("substr", slice_fn);

    hbs.registerHelper("safe_anchor_id", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        std::ranges::transform(res, res.begin(), [](char c) {
            if (c == ' ' || c == '_')
            {
                return '-';
            }
           return toLower(c);
        });
        // remove any ":" from the string
        auto it = std::remove(res.begin(), res.end(), ':');
        res.erase(it, res.end());
        return res;
    }));

    hbs.registerHelper("strip_namespace", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value fn = options["fn"];
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            res = firstArg.getString();
        }
        int inside = 0;
        size_t count = 0;
        size_t offset = std::string::npos;
        for (char c: res)
        {
            switch (c)
            {
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
        if (offset != std::string::npos)
        {
            return res.substr(offset);
        }
        else
        {
            return res;
        }
    }));
}

void
registerContainerHelpers(Handlebars& hbs)
{
    static auto size_fn = dom::makeInvocable([](
        dom::Value const& val)
    {
        return val.size();
    });

    hbs.registerHelper("size", size_fn);
    hbs.registerHelper("len", size_fn);

    static auto keys_fn = dom::makeInvocable([](
        dom::Value container) -> dom::Value
    {
        if (!container.isObject())
        {
            return container;
        }
        auto const& obj = container.getObject();
        dom::Array res;
        for (auto const& [key, _]: obj)
        {
            res.emplace_back(key);
        }
        return res;
    });

    hbs.registerHelper("keys", keys_fn);
    hbs.registerHelper("list", keys_fn);
    hbs.registerHelper("iter", keys_fn);

    static auto values_fn = dom::makeInvocable([](
        dom::Value container) -> dom::Value
    {
        if (!container.isObject())
        {
            return container;
        }
        auto const& obj = container.getObject();
        dom::Array res;
        for (auto const& [_, value]: obj)
        {
            res.emplace_back(value);
        }
        return res;
    });

    hbs.registerHelper("values", values_fn);

    static auto del_fn = dom::makeInvocable([](
        dom::Value range, dom::Value const& item) -> dom::Value {
        if (range.isArray())
        {
            auto const& arr = range.getArray();
            auto const& val = item;
            dom::Array res;
            auto const n = static_cast<std::int64_t>(arr.size());
            for (std::int64_t i = 0; i < n; i++)
            {
                if (arr[i] != val)
                {
                    res.emplace_back(arr.at(i));
                }
            }
            return res;
        }
        else if (range.isObject())
        {
            auto const& obj = range.getObject();
            auto const& key = item.getString();
            dom::Object res;
            for (auto const& [k, v]: obj)
            {
                if (k != key)
                {
                    res.set(k, v);
                }
            }
            return res;
        }
        else
        {
            return range;
        }
    });

    hbs.registerHelper("del", del_fn);
    hbs.registerHelper("delete", del_fn);

    static auto has_fn = dom::makeInvocable([](
        dom::Value const& ctx, dom::Value const& prop)
    {
        if (ctx.isObject())
        {
            dom::Value const& objV = ctx;
            auto const& obj = objV.getObject();
            auto const& key = prop.getString();
            return obj.exists(key);
        }
        else if (ctx.isArray())
        {
            dom::Value const& arrV = ctx;
            auto const& arr = arrV.getArray();
            auto const& value = prop;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                if (arr[i] == value)
                {
                    return true;
                }
            }
        }
        return false;
    });

    hbs.registerHelper("has", has_fn);
    hbs.registerHelper("exist", has_fn);
    hbs.registerHelper("contains", has_fn);

    static auto has_any_fn = dom::makeInvocable([](
        dom::Value const& container, dom::Value const& item)
    {
        if (container.isObject())
        {
            auto const& obj = container.getObject();
            dom::Value const& keysV = item;
            auto const& keys = keysV.getArray();
            auto const n = static_cast<std::int64_t>(keys.size());
            for (std::int64_t i = 0; i < n; ++i)
            {
                dom::Value k = keys[i];
                if (obj.exists(k.getString()))
                {
                    return true;
                }
            }
            return false;
        }
        else if (container.isArray())
        {
            auto const& arr = container.getArray();
            auto const& values = item.getArray();
            auto n = static_cast<std::int64_t>(values.size());
            for (std::int64_t i = 0; i < n; ++i)
            {
                std::size_t const n2 = arr.size();
                for (std::size_t j = 0; j < n2; ++j)
                {
                    dom::Value a = arr[j];
                    dom::Value b = values[i];
                    if (a == b)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
        else
        {
            return false;
        }
    });

    hbs.registerHelper("has_any", has_any_fn);
    hbs.registerHelper("exist_any", has_any_fn);
    hbs.registerHelper("contains_any", has_any_fn);

    static auto get_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::size_t const n = arguments.size();
        dom::Value container = arguments.at(0);
        dom::Value field = arguments.at(1);
        dom::Value default_value = nullptr;
        if (n > 3)
        {
            default_value = arguments.at(2);
        }

        if (container.isArray())
        {
            auto const& arr = container.getArray();
            auto index = field.getInteger();
            if (index < 0)
            {
                index = normalize_index(index, static_cast<std::int64_t>(arr.size()));
            }
            if (index >= static_cast<std::int64_t>(arr.size()))
            {
                return default_value;
            }
            return arr.at(index);
        }
        else if (container.isObject())
        {
            auto const& obj = container.getObject();
            auto const& key = field.getString();
            if (obj.exists(key))
            {
                return obj.find(key);
            }
            return default_value;
        }
        else
        {
            return default_value;
        }
    });

    hbs.registerHelper("get", get_fn);
    hbs.registerHelper("get_or", get_fn);

    static auto items_fn = dom::makeInvocable([](
        dom::Value items) -> dom::Value
    {
        if (items.isObject())
        {
            auto const& obj = items.getObject();
            dom::Array res;
            for (auto const& [key, value]: obj)
            {
                dom::Array item;
                item.emplace_back(key);
                item.emplace_back(value);
                res.emplace_back(item);
            }
            return res;
        }
        else
        {
            return items;
        }
    });

    hbs.registerHelper("items", items_fn);
    hbs.registerHelper("entries", items_fn);

    static auto first_fn = dom::makeInvocable([](
        dom::Value range) -> dom::Value {
        if (range.isArray())
        {
            auto const& arr = range.getArray();
            if (arr.empty())
            {
                return nullptr;
            }
            return arr.at(0);
        }
        else if (range.isObject())
        {
            auto const& obj = range.getObject();
            if (obj.empty())
            {
                return nullptr;
            }
            return obj.get(0).value;
        }
        else
        {
            return range;
        }
    });

    hbs.registerHelper("first", first_fn);
    hbs.registerHelper("head", first_fn);
    hbs.registerHelper("front", first_fn);

    static auto last_fn = dom::makeInvocable([](
        dom::Value range) -> dom::Value
    {
        if (range.isArray())
        {
            auto const& arr = range.getArray();
            if (arr.empty())
            {
                return {};
            }
            return arr.back();
        }
        else if (range.isObject())
        {
            auto const& obj = range.getObject();
            if (obj.empty())
            {
                return nullptr;
            }
            return obj.get(obj.size() - 1).value;
        }
        else
        {
            return range;
        }
    });

    hbs.registerHelper("last", last_fn);
    hbs.registerHelper("tail", last_fn);
    hbs.registerHelper("back", last_fn);

    static auto reverse_fn = dom::makeInvocable([](
        dom::Value container) -> dom::Value
    {
        if (container.isArray())
        {
            auto const& arr = container.getArray();
            dom::Array res;
            for (std::size_t i = arr.size(); i > 0; --i)
            {
                res.emplace_back(arr.at(i - 1));
            }
            return res;
        }
        else if (container.isObject())
        {
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
        }
        else
        {
            return container;
        }
    });

    hbs.registerHelper("reverse", reverse_fn);
    hbs.registerHelper("reversed", reverse_fn);

    static auto update_fn = dom::makeInvocable([](
        dom::Value container, dom::Value const& items) -> dom::Value {
        if (container.isObject())
        {
            auto const& obj = container.getObject();
            auto const& other = items.getObject();
            dom::Object res = createFrame(obj);
            for (auto const& [k, v]: other)
            {
                res.set(k, v);
            }
            return res;
        }
        else if (container.isArray())
        {
            auto const& arr = container.getArray();
            auto const& other = items.getArray();
            dom::Array res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                res.emplace_back(arr.at(i));
            }
            std::size_t const n2 = other.size();
            for (std::size_t i = 0; i < n2; ++i)
            {
                bool arr_contains = false;
                std::size_t const n3 = res.size();
                for (std::size_t j = 0; j < n3; ++j)
                {
                    if (res.at(j) == other.at(i))
                    {
                        arr_contains = true;
                        break;
                    }
                }
                if (!arr_contains)
                {
                    res.emplace_back(other.at(i));
                }
            }
            return res;
        }
        else
        {
            return container;
        }
    });

    hbs.registerHelper("update", update_fn);
    hbs.registerHelper("merge", update_fn);

    static auto sort_fn = dom::makeInvocable([](
        dom::Value container) -> dom::Value
    {
        if (container.isArray())
        {
            auto const& arr = container.getArray();
            std::vector<dom::Value> res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                res.emplace_back(arr.at(i));
            }
            std::ranges::sort(res, [](auto const& a, auto const& b) {
                return a < b;
            });
            dom::Array res2;
            for (const auto & re : res) {
                res2.emplace_back(re);
            }
            return res2;
        }
        else
        {
            return container;
        }
    });

    hbs.registerHelper("sort", sort_fn);

    static auto sort_by_fn = dom::makeInvocable([](
        dom::Value container, dom::Value const& keyV) -> dom::Value
    {
        // Given an array of objects, sort these objects by their value at
        // a given key
        if (container.isArray())
        {
            auto const& arr = container.getArray();
            auto const& key = keyV.getString();
            std::vector<dom::Value> res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i) {
                res.emplace_back(arr.at(i));
            }
            std::ranges::sort(res, [&key](dom::Value const& a, dom::Value const& b)
            {
                // If the value is not an object, then we can't sort it
                // by a key, so we just sort it by the value itself
                if (!a.isObject() || !b.isObject())
                {
                    if (a.isObject())
                    {
                        return true;
                    }
                    if (b.isObject())
                    {
                        return false;
                    }
                    return a < b;
                }
                // If the value is an object but the key doesn't exist,
                // then we can't sort it by a key, so we just sort it by
                // whichever has the key
                const bool ak = a.getObject().exists(key);
                const bool bk = b.getObject().exists(key);
                if (!ak)
                {
                    return bk;
                }
                if (!bk)
                {
                    return false;
                }
                // If the value is an object and the key exists, then we
                // sort it by the value at the key
                return a.getObject().find(key) < b.getObject().find(key);
            });
            return dom::Array(res);
        }
        else
        {
            // If the value is not an array, then we can't sort it
            return container;
        }
    });

    hbs.registerHelper("sort_by", sort_by_fn);

    hbs.registerHelper("at", dom::makeInvocable(at_fn));

    static auto fill_fn = dom::makeInvocable([](
        dom::Value container,
        dom::Value const& fill_value,
        dom::Value const& startV,
        dom::Value const& stopV) -> dom::Value {
        if (container.isArray())
        {
            auto const& arr = container.getArray();
            std::int64_t start = 0;
            if (startV.isInteger())
            {
                start = startV.getInteger();
            }
            auto const n = static_cast<std::int64_t>(arr.size());
            auto stop = n;
            if (stopV.isInteger())
            {
                stop = stopV.getInteger();
            }
            start = normalize_index(start, n);
            stop = normalize_index(stop, n);
            dom::Array res;
            for (std::int64_t i = 0; i < n; ++i)
            {
                if (i >= start && i < stop)
                {
                    res.emplace_back(fill_value);
                }
                else
                {
                    res.emplace_back(arr.at(i));
                }
            }
            return res;
        }
        else
        {
            // If the value is not an array, then we can't fill it
            return container;
        }
    });

    hbs.registerHelper("fill", fill_fn);

    hbs.registerHelper("count", dom::makeVariadicInvocable(count_fn));
    hbs.registerHelper("replace", dom::makeVariadicInvocable(replace_fn));

    hbs.registerHelper("chunk", dom::makeInvocable([](
        dom::Value range, dom::Value const& sizeV) -> dom::Value
    {
        std::int64_t chunkSize = sizeV.getInteger();
        if (range.isArray())
        {
            auto const& arr = range.getArray();
            dom::Array res;
            std::int64_t i = 0;
            auto const n = static_cast<std::int64_t>(arr.size());
            while (i < n)
            {
                dom::Array chunk;
                for (std::int64_t j = 0; j < chunkSize && i < n; ++j)
                {
                    chunk.emplace_back(arr.at(i));
                    ++i;
                }
                res.emplace_back(chunk);
            }
            return res;
        }
        else if (range.isString())
        {
            auto const& str = range.getString();
            dom::Array res;
            std::int64_t i = 0;
            auto const n = static_cast<std::int64_t>(str.size());
            while (i < n)
            {
                std::string chunk;
                for (std::int64_t j = 0; j < chunkSize && i < n; ++j)
                {
                    chunk += str.get()[i];
                    ++i;
                }
                res.emplace_back(chunk);
            }
            return res;
        }
        else if (range.isObject())
        {
            auto const& obj = range.getObject();
            dom::Array res;
            std::size_t i = 0;
            std::size_t const n = obj.size();
            while (i < n)
            {
                dom::Object chunk;
                std::int64_t j = 0;
                while (j < chunkSize && i < n)
                {
                    auto const& [k, v] = obj.at(i);
                    chunk.set(k, v);
                    ++i;
                    ++j;
                }
                res.emplace_back(chunk);
            }
            return res;
        }
        else
        {
            return range;
        }
    }));

    hbs.registerHelper("group_by", dom::makeInvocable([](
        dom::Value range, dom::Value const& keyV) -> dom::Value
    {
        // Given an array of objects, group these objects by a key in each them
        // The result is an object with keys being the values of the key in each
        // object, and the values being arrays of objects with that key value
        if (!range.isArray())
        {
            return range;
        }
        dom::Array const& array = range.getArray();
        std::string key(keyV.getString());
        dom::Array::size_type n = array.size();
        std::vector<std::uint8_t> copied(n, 0x00);
        dom::Object res;
        for (std::size_t i = 0; i < n; ++i)
        {
            if (copied[i] != 0x00 || !array[i].isObject() || !array[i].getObject().exists(key))
            {
                // object already copied or doesn't have the key
                copied[i] = 0x01;
                continue;
            }
            copied[i] = 0x01;

            // Create a group for this key value
            std::string group_name(toString(array[i][key]));
            dom::Array group;
            group.emplace_back(array[i]);

            // Copy any other equivalent keys to the same group
            for (std::size_t j = i; j < n; ++j)
            {
                if (copied[j])
                {
                    continue;
                }
                if (array[j][key].getString() == array[i][key].getString())
                {
                    group.emplace_back(array[j]);
                    copied[j] = 0x01;
                }
            }
            res.set(group_name, group);
        }
        return res;
    }));

    hbs.registerHelper("pluck", dom::makeInvocable([](
        dom::Value rangeV, dom::Value const& keyV) -> dom::Value
    {
        if (!rangeV.isArray())
        {
            return rangeV;
        }
        // Given an array of objects, take the value of a key from each object
        dom::Array const& range = rangeV.getArray();
        std::string key(keyV.getString());
        dom::Array res;
        auto n = static_cast<std::int64_t>(range.size());
        for (std::int64_t i = 0; i < n; ++i)
        {
            if (range[i].isObject() && range[i].getObject().exists(key))
            {
                res.emplace_back(range[i].getObject().find(key));
            }
        }
        return res;
    }));

    hbs.registerHelper("unique", dom::makeInvocable([](
        dom::Value rangeV) -> dom::Value
    {
        if (!rangeV.isArray())
        {
            return rangeV;
        }
        // remove duplicates from an array
        dom::Array const& range = rangeV.getArray();
        std::vector<dom::Value> res;
        for (const auto& el: range)
        {
            res.push_back(el);
        }
        std::ranges::sort(res, [](dom::Value const& a, dom::Value const& b)
        {
            return a < b;
        });
        auto [first, last] = std::ranges::unique(res, [](
            dom::Value const& a, dom::Value const& b)
        {
            return a == b;
        });
        res.erase(first, res.end());
        dom::Array res2;
        for (auto const& v : res) {
            res2.emplace_back(v);
        }
        return res2;
    }));

    hbs.registerHelper("concat", dom::makeInvocable(concat_fn));
}

} // helpers

void
Handlebars::
unregisterHelper(std::string_view name) {
    auto it = helpers_.find(name);
    if (it != helpers_.end())
        helpers_.erase(it);

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

} // mrdox
} // clang

