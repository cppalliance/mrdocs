//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_HTMLTAG_HPP
#define MRDOX_TOOL_HTMLTAG_HPP

#include <mrdox/Support/String.hpp>
#include <fmt/format.h>
#include <concepts>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>


namespace clang {
namespace mrdox {
namespace html {

struct stringish
{
    stringish() = default;

    template<typename T>
        requires std::constructible_from<std::string_view, T>
    stringish(T&& t)
        : Underlying(std::string_view(std::forward<T>(t)))
    {
    }

    template<typename T>
        requires (! std::constructible_from<std::string_view, T> &&
            std::constructible_from<std::string, T>)
    stringish(T&& t)
        : Underlying(std::forward<T>(t))
    {
    }

    std::string Underlying;
};

template<typename T>
struct vectorish
{
    vectorish() = default;

    template<typename U>
        requires std::convertible_to<U, T>
    vectorish(U&& elem)
        : Underlying({elem})
    {
    }

    vectorish(const std::initializer_list<T>& init)
        : Underlying(init)
    {
    }

    std::vector<T> Underlying;
};

struct HTMLTag
{
    std::string_view Name;
    std::string Id;
    vectorish<std::string_view> Class;
    vectorish<std::pair<
        std::string_view,
        std::string>> Attrs;
    stringish Content;
};

struct HTMLTagWriter
{
    HTMLTag Tag;

    HTMLTagWriter() = default;

    HTMLTagWriter(
        const HTMLTag& tag)
        : Tag(tag)
    {
    }

    HTMLTagWriter(
        HTMLTag&& tag)
        : Tag(std::move(tag))
    {
    }

    HTMLTagWriter(
        std::string_view content)
        : HTMLTagWriter({.Content = content})
    {
    }

    template<typename... Args>
    HTMLTagWriter& write(
        std::string_view content,
        Args&&... args)
    {
        // write(content);
        Tag.Content.Underlying += content;
        stale_ = true;
        if constexpr(sizeof...(Args))
            write(std::forward<Args>(args)...);
        return *this;
    }

#if 0
    template<typename... Args>
    HTMLTagWriter& write(
        const HTMLTagWriter& child_tag,
        Args&&... args)
    {
        write(std::string(child_tag));
        if constexpr(sizeof...(Args))
            write(std::forward<Args>(args)...);
        return *this;
    }

    template<typename... Args>
    HTMLTagWriter& write(
        const HTMLTag& child_tag,
        Args&&... args)
    {
        write(HTMLTagWriter(child_tag));
        if constexpr(sizeof...(Args))
            write(std::forward<Args>(args)...);
        return *this;
    }
#else
    HTMLTagWriter& write(
        const HTMLTagWriter& child_tag)
    {
        write(std::string(child_tag));
        return *this;
    }

    HTMLTagWriter& write(
        const HTMLTag& child_tag)
    {
        write(HTMLTagWriter(child_tag));
        return *this;
    }
#endif

    operator std::string() const
    {
        if(stale_)
            return build();
        return cached_;
    }

    operator std::string&()
    {
        if(stale_)
        {
            cached_ = build();
            stale_ = false;
        }
        return cached_;
    }

    operator std::string_view()
    {
        if(stale_)
        {
            cached_ = build();
            stale_ = false;
        }
        return cached_;
    }

    bool has_content() const noexcept
    {
        return ! Tag.Content.Underlying.empty();
    }

    bool has_tag() const noexcept
    {
        return ! Tag.Name.empty();
    }

private:
    std::string build() const
    {
        if(Tag.Name.empty())
            return Tag.Content.Underlying;

        std::string r;
        r += fmt::format("<{}", Tag.Name);
        if(! Tag.Id.empty())
            r += fmt::format(" id = \"{}\"", Tag.Id);
        const auto& Class = Tag.Class.Underlying;
        if(! Class.empty())
        {
            r += fmt::format(" class = \"{}", Class.front());
            for(auto c : std::span(Class.begin() + 1, Class.end()))
                r += fmt::format(" {}", c);
            r += '"';
        }
        for(auto&& [attr, val] : Tag.Attrs.Underlying)
            r += fmt::format(" {} = \"{}\"", attr, val);
        if(Tag.Content.Underlying.empty())
            r += "/>";
        else
            r += fmt::format(">{}</{}>",
                Tag.Content.Underlying, Tag.Name);
        return r;
    }

    std::string cached_;
    bool stale_ = true;
};

} // html
} // mrdox
} // clang

#endif
