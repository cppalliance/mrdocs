//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/SplitLines.hpp>
#include <mrdocs/Support/String.hpp>
#include <limits>

namespace mrdocs {

void
replace(std::string& s, std::string_view from, std::string_view to)
{
    if (from.empty())
        return;

    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

namespace {
// Return true if c is space or tab
constexpr
bool
isSpaceOrTab(char c) noexcept { return c == ' ' || c == '\t'; }

// Return true if s ends with a line break sequence
constexpr
bool
endsWithLineBreak(std::string_view s) noexcept
{
    // Recognize if s ends exactly at a line break sequence handled by lbLen
    for (std::size_t k = 1; k <= 3 && k <= s.size(); ++k)
    {
        const std::size_t i = s.size() - k;
        if (detail::lbLen(s, i) == k) return true;
    }
    return false;
}
} // namespace

std::string
reindentCode(std::string_view code, std::size_t indent)
{
    auto lines = splitLines(code);

    // 1) Compute common indentation (spaces/tabs) across non-blank lines
    std::size_t common = std::numeric_limits<std::size_t>::max();
    bool sawContent = false;
    for (std::string_view line : lines)
    {
        auto it = std::ranges::find_if_not(line, isSpaceOrTab);
        if (it == line.end())
        {
            // blank/whitespace-only -> skip
            continue;
        }
        auto lead = static_cast<std::size_t>(it - line.begin());
        common = std::min(common, lead);
        sawContent = true;
    }
    if (!sawContent)
    {
        // all blank lines -> nothing to do
        common = 0;
    }

    // 2) Build output: remove common, add requested indent to non-blank lines
    std::string out;
    out.reserve(code.size()); // heuristic; result is often similar size
    const std::string indentStr(indent, ' ');
    const bool hadTrailingLB = endsWithLineBreak(code);

    bool first = true;
    for (std::string_view line : lines) {
        if (!first)
        {
            out.push_back('\n');
        }
        first = false;

        // Keep blank lines blank (no added indentation)
        auto it = std::ranges::find_if_not(line, isSpaceOrTab);
        if (it == line.end())
        {
            // blank -> nothing to append on this line
            continue;
        }

        auto lead = static_cast<std::size_t>(it - line.begin());
        std::size_t remove = std::min(common, lead);
        std::string_view tail = line.substr(remove);

        out.append(indentStr);
        out.append(tail.data(), tail.size());
    }
    if (hadTrailingLB)
    {
        out.push_back('\n');
    }
    return out;
}

} // mrdocs
