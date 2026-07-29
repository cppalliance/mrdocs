//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_STRING_SPLITLINES_HPP
#define MRDOCS_API_SUPPORT_STRING_SPLITLINES_HPP

#include <mrdocs/Platform.hpp>
#include <cstddef>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

namespace mrdocs {

namespace detail {
// Return length (in bytes) of the line-break sequence starting at i, or 0 if
// none
constexpr std::size_t
lbLen(std::string_view s, std::size_t i) noexcept
{
    auto b = [&](std::size_t k) -> unsigned char {
        return static_cast<unsigned char>(s[k]);
    };
    if (i >= s.size())
    {
        return 0;
    }

    // CRLF
    if (s[i] == '\r')
    {
        if (i + 1 < s.size() && s[i + 1] == '\n')
        {
            return 2;
        }
        return 1; // lone CR
    }
    // LF, VT, FF
    if (s[i] == '\n' || s[i] == '\v' || s[i] == '\f')
    {
        return 1;
    }

    // NEL: U+0085 — single byte (0x85) or UTF-8 C2 85
    if (b(i) == 0x85)
    {
        return 1;
    }
    if (i + 1 < s.size() && b(i) == 0xC2 && b(i + 1) == 0x85)
    {
        return 2;
    }

    // LS (U+2028) and PS (U+2029): UTF-8 E2 80 A8 / E2 80 A9
    if (i + 2 < s.size() && b(i) == 0xE2 && b(i + 1) == 0x80
        && (b(i + 2) == 0xA8 || b(i + 2) == 0xA9))
    {
        return 3;
    }

    return 0;
}
}

/** A lazy input range of std::string_view lines split on all known line breaks.
*/
struct SplitLinesView : std::ranges::view_interface<SplitLinesView> {
    /** Underlying string to split.
    */
    std::string_view sv_;

    /** Construct an empty view.
    */
    constexpr SplitLinesView() = default;
    /** Construct a view over @p sv.
        @param sv String to split into lines.
    */
    explicit constexpr SplitLinesView(std::string_view sv) : sv_(sv) {}

    /** Iterator over lines produced by SplitLinesView.
    */
    struct Iterator {
        /** Reference to the source string.
        */
        std::string_view sv{};
        /** Start of the current line.
        */
        std::size_t cur = 0;       // start of current line
        /** Index of the next break delimiter or npos.
        */
        std::size_t nextBreak = 0; // index of current break (or npos)
        /** Flag indicating the end iterator.
        */
        bool atEnd = false;

        /** Construct an end iterator.
        */
        constexpr Iterator() = default;
        /** Construct a begin or end iterator.
            @param s Source string.
            @param begin If true, position at the first line; otherwise create end iterator.
        */
        explicit constexpr Iterator(std::string_view s, bool begin) : sv(s)
        {
            if (!begin)
            {
                atEnd = true;
                return;
            }
            nextBreak = findBreak(cur);
        }

        /** Compute the next line break position.
            @param from Index to start searching.
            @return Offset of the next break or npos.
        */
        constexpr std::size_t
        findBreak(std::size_t from) const noexcept
        {
            for (std::size_t i = from; i < sv.size(); ++i)
            {
                if (detail::lbLen(sv, i))
                {
                    return i;
                }
            }
            return std::string_view::npos;
        }

        /** Line view type exposed by the iterator.
        */
        using value_type = std::string_view;
        /** Signed distance type for the iterator.
        */
        using difference_type = std::ptrdiff_t;

        /** Return the current line segment.
            @return View of the current line.
        */
        constexpr value_type
        operator*() const noexcept
        {
            auto end = (nextBreak == std::string_view::npos) ?
                           sv.size() :
                           nextBreak;
            return sv.substr(cur, end - cur);
        }

        /** Advance to the next line.
            @return Reference to this iterator.
        */
        constexpr Iterator&
        operator++() noexcept
        {
            if (nextBreak == std::string_view::npos)
            {
                atEnd = true;
                return *this;
            }
            std::size_t const jump = detail::lbLen(sv, nextBreak);
            cur = nextBreak + jump;
            nextBreak = findBreak(cur);
            if (cur > sv.size())
            {
                atEnd = true;
            }
            return *this;
        }

        /** Advance to the next line (post-increment).
            @param unused Dummy parameter for postfix form.
        */
        constexpr void
        operator++(int unused)
        {
            (void)unused;
            ++*this;
        }

        friend constexpr bool
        operator==(Iterator const& it, std::default_sentinel_t) noexcept
        {
            return it.atEnd;
        }
    };

    /** Return an iterator to the first line.
        @return Iterator positioned at the first line.
    */
    constexpr Iterator
    begin() const noexcept
    {
        return Iterator{ sv_, true };
    }
    /** Return the end sentinel for the view.
        @return Default sentinel representing the end.
    */
    constexpr std::default_sentinel_t
    end() const noexcept
    {
        return {};
    }
};

// Pipeable range adaptor object:
//   - s | text::views::splitLines
//   - text::views::splitLines(s)
/** Range adaptor that constructs a SplitLinesView.
*/
struct SplitLinesAdaptor {
    // Call-style
    /** Split a string view into lines.
        @param sv Source string.
        @return View over the lines in @p sv.
    */
    constexpr auto
    operator()(std::string_view sv) const
    {
        return SplitLinesView{ sv };
    }

    // Accept any contiguous range of (const) char and wrap it as string_view.
    template <std::ranges::contiguous_range R>
    requires
        std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, char>
    /** Split any contiguous character range into lines.
        @param r Range of characters.
        @return View over the lines in the range.
    */
    constexpr auto
    operator()(R&& r) const
    {
        return SplitLinesView{
            std::string_view(std::ranges::data(r), std::ranges::size(r))
        };
    }

    // Pipe-style
    template <std::ranges::viewable_range R>
    requires
        std::ranges::contiguous_range<R> &&
        std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, char>
    /** Pipe a contiguous character range into the adaptor.
        @param r Range to split.
        @param a SplitLines adaptor instance.
        @return View over the lines in @p r.
    */
    friend constexpr auto
    operator|(R&& r, SplitLinesAdaptor const& a)
    {
        return a(std::forward<R>(r));
    }
};

/** Split a string view into lines, recognizing all common line breaks

    This is a convenience function for creating a SplitLinesView.
*/
inline constexpr SplitLinesAdaptor splitLines{};

} // mrdocs

#endif
