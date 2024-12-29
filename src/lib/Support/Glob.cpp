//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <ranges>
#include <llvm/ADT/BitVector.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/GlobPattern.h>
#include <mrdocs/Support/Glob.hpp>
#include <llvm/Support/YAMLTraits.h>

namespace clang::mrdocs {

namespace {

/*  Expand character ranges in S and return a bitmap.

    For example, "a-cf-hz" is expanded into a bitmap
    representing "abcfghz".

    This function is a modified version of the expand function from
    llvm/lib/Support/GlobPattern.cpp.

    @param str The string to expand.
    @param original The original string to use in error messages.
 */
Expected<llvm::BitVector>
parseCharRange(
    std::string_view str,
    std::string_view original)
{
    llvm::BitVector BV(256, false);

    // Expand X-Y.
    while (str.size() >= 3)
    {
        std::uint8_t const charRangeStart = str[0];
        std::uint8_t const charRangeEnd = str[2];

        if (str[1] != '-')
        {
            // Not "X-Y": consume the first character
            BV[charRangeStart] = true;
            str = str.substr(1);
            continue;
        }

        if (charRangeStart > charRangeEnd)
        {
            // Invalid range.
            return Unexpected(formatError(
                "invalid glob pattern: {}", original));
        }

        for (int c = charRangeStart; c <= charRangeEnd; ++c)
        {
            BV[static_cast<std::uint8_t>(c)] = true;
        }
        str = str.substr(3);
    }

    for (char const c: str)
    {
        BV[static_cast<uint8_t>(c)] = true;
    }
    return BV;
}

/* Expand brace expansions in a string and return a list of patterns.

    For example, "a{b,c}d" is expanded into "abd" and "acd".

    This function is a modified version of the parseBraceExpansions function from
    llvm/lib/Support/GlobPattern.cpp.

    @param str The string to expand.
    @param max The maximum number of subpatterns to generate.
 */
Expected<llvm::SmallVector<std::string, 1>>
parseBraceExpansions(
    std::string_view str,
    std::optional<std::size_t> const max)
{
    // If there are no brace expansions, return the original string
    // as the only subpattern.
    llvm::SmallVector SubPatterns = {std::string(str)};

    // Parse brace expansions.
    struct BraceExpansion
    {
        // Start of the brace expansion.
        std::size_t start{0};
        // Length of the brace expansion.
        std::size_t length{0};
        // Terms in the brace expansion, such as "a,b,c".
        llvm::SmallVector<std::string_view, 2> terms;
    };
    llvm::SmallVector<BraceExpansion, 0> expansions;

    // The current brace expansion being parsed and populated
    BraceExpansion *curBraces = nullptr;
    // The start of the contents of the current brace expansion
    std::size_t curBracesBegin{0};
    for (size_t i = 0, end = str.size(); i != end; ++i)
    {
        char const c = str[i];

        // Skip character ranges.
        if (c == '[')
        {
            i = str.find(']', i + 2);
            if (i == std::string::npos)
            {
                return Unexpected(Error("invalid glob pattern, unmatched '['"));
            }
            continue;
        }

        // Parse brace expansions
        if (c == '{')
        {
            if (curBraces)
            {
                return Unexpected(Error("nested brace expansions are not supported"));
            }
            // Start a new brace expansion.
            expansions.emplace_back();
            curBraces = &expansions.back();
            curBraces->start = i;
            curBracesBegin = i + 1;
            continue;
        }

        // If we are not in a brace expansion, continue.
        // Otherwise, parse the brace expansion.
        if (c == ',')
        {
            if (!curBraces)
            {
                continue;
            }
            // Add a term to the current brace expansion.
            std::string_view const term = str.substr(
                curBracesBegin, i - curBracesBegin);
            curBraces->terms.push_back(term);
            curBracesBegin = i + 1;
            continue;
        }

        // If we are not in a brace expansion, continue.
        // Otherwise, finish the brace expansion.
        if (c == '}')
        {
            if (!curBraces)
            {
                continue;
            }
            if (curBraces->terms.empty())
            {
                if (i == curBracesBegin)
                {
                    return Unexpected(Error("empty brace expansions are not supported"));
                }
                return Unexpected(Error("singleton brace expansions are not supported"));
            }
            // Add the last term to the current brace expansion.
            std::string_view const term = str.substr(
                curBracesBegin, i - curBracesBegin);
            curBraces->terms.push_back(term);
            curBraces->length = i - curBraces->start + 1;
            // Reset the current brace expansion.
            curBraces = nullptr;
            continue;
        }

        // Skip escaped characters.
        if (c == '\\')
        {
            if (++i == end)
            {
                return Unexpected(Error("invalid glob pattern, stray '\\'"));
            }
        }
    }

    // Check for incomplete brace expansions.
    if (curBraces)
    {
        return Unexpected(Error("incomplete brace expansion"));
    }

    // Check number of subpatterns.
    if (max)
    {
        std::size_t nSubPatterns = 1;
        for (auto& expansion: expansions)
        {
            if (nSubPatterns > std::numeric_limits<std::size_t>::max() / expansion.terms.size())
            {
                nSubPatterns = std::numeric_limits<std::size_t>::max();
                break;
            }
            nSubPatterns *= expansion.terms.size();
        }
        if (max && nSubPatterns > *max)
        {
            return Unexpected(Error("too many brace expansions"));
        }
    }

    // Replace brace expansions in reverse order so that we don't invalidate
    // earlier start indices
    for (auto& [start, length, terms]: std::ranges::views::reverse(expansions))
    {
        llvm::SmallVector<std::string> OrigSubPatterns;
        std::swap(SubPatterns, OrigSubPatterns);
        for (std::string_view Term: terms)
        {
            for (std::string_view Orig: OrigSubPatterns)
            {
                SubPatterns.emplace_back(Orig)
                    .replace(start, length, Term);
            }
        }
    }
    return std::move(SubPatterns);
}

/* A glob pattern without any {}
 */
struct SubGlobPattern {
    struct CharBracket {
        std::size_t next_offset_;
        llvm::BitVector bytes_;

        bool
        match(std::uint8_t const c) const
        {
            return bytes_[c];
        }

        bool
        match(char const c) const
        {
            return bytes_[static_cast<std::uint8_t>(c)];
        }
    };

    enum class MatchType {
        FULL,
        PARTIAL,
        MISMATCH
    };

    static
    Expected<SubGlobPattern>
    create(std::string_view pattern);

    MatchType
    match(std::string_view str, char delimiter) const;

    std::string_view
    pattern() const {
        return pattern_;
    }

    // The original glob pattern.
    std::string pattern_;

    // The expanded char brackets.
    std::vector<CharBracket> brackets_;
};

/* Create a new SubGlobPattern from a glob pattern.

    This function takes a glob pattern and creates a new SubGlobPattern
    from it. The glob pattern is parsed and expanded into a list of
    subpatterns that are used for matching.

    This function is a modified version of the create function from
    llvm/lib/Support/GlobPattern.cpp.

    @param pattern The glob pattern to use for matching.
 */
Expected<SubGlobPattern>
SubGlobPattern::
create(std::string_view pattern)
{
    SubGlobPattern res;

    // Parse brackets.
    res.pattern_.assign(pattern.begin(), pattern.end());
    for (std::size_t i = 0, end = pattern.size(); i != end; ++i)
    {
        if (pattern[i] == '[')
        {
            // Check for a character range.
            ++i;
            // `]` is allowed as the first character of a character class.
            // `[]` is invalid.
            // So, just skip the first character.
            std::size_t const charRangeEnd = pattern.find(']', i + 1);
            if (charRangeEnd == std::string_view::npos)
            {
                if (pattern[i] == ']')
                {
                    return Unexpected(Error("invalid glob pattern, empty character range"));
                }
                return Unexpected(Error("invalid glob pattern, unmatched '['"));
            }
            std::string_view rangeChars = pattern.substr(i, charRangeEnd - i);
            bool const invert = pattern[i] == '^' || pattern[i] == '!';
            rangeChars = rangeChars.substr(invert);
            MRDOCS_TRY(llvm::BitVector BV, parseCharRange(rangeChars, pattern));
            if (invert)
            {
                BV.flip();
            }
            CharBracket bracket{ charRangeEnd + 1, std::move(BV) };
            res.brackets_.push_back(std::move(bracket));
            i = charRangeEnd;
        }
        else if (pattern[i] == '\\')
        {
            if (++i == end)
            {
                return Unexpected(Error("invalid glob pattern, stray '\\'"));
            }
        }
    }
    return res;
}

/*  Match a string against the subglob pattern.

    This function matches a string against the subglob pattern. The string is
    matched against each segment of the pattern. If a segment is matched, the
    next segment is matched against the remaining string. If a segment is not
    matched, the function backtracks to the last '*' and tries to match the
    remaining string against the next segment.

    This function is a modified version of the match function from
    llvm/lib/Support/GlobPattern.cpp.

    @param str The string to match against the pattern.
 */
SubGlobPattern::MatchType
SubGlobPattern::
match(std::string_view str, char const delimiter) const
{
    // The current position in the pattern we're matching.
    std::string_view pattern = pattern_;

    // The index of the current parsed brackets we're using.
    std::size_t bracketsIdx = 0;

    // The string we're matching "*".
    std::string_view starStr = str;

    // The pattern suffix after the "*" run.
    std::optional<std::string_view> patternStarSuffix;

    // The saved brackets index for backtracking.
    std::size_t bracketsStarMatchIdx = 0;

    // Whether the current "*" run is a double star.
    bool isDoubleStar = false;

    // Attempt to match a single character in the string.
    auto matchOne = [&]() {
        if (pattern.front() == '*')
        {
            // Move the pattern iterator past the '*'s.
            pattern.remove_prefix(1);
            isDoubleStar = false;
            while (!pattern.empty() && pattern.front() == '*')
            {
                pattern.remove_prefix(1);
                isDoubleStar = true;
            }
            // Save the positions where we started the "*" segment
            // to be used by backtracking if we see a mismatch later.
            patternStarSuffix = pattern;
            starStr = str;
            bracketsStarMatchIdx = bracketsIdx;
            return true;
        }

        if (pattern.front() == '[')
        {
            // Match a bracket expression. The parsed bracket expression is
            // stored in brackets_ at the current bracketsIdx.
            if (CharBracket const& bracketExp = brackets_[bracketsIdx];
                bracketExp.match(str.front()))
            {
                pattern = std::string_view(pattern_.data()).substr(bracketExp.next_offset_);
                ++bracketsIdx;
                str.remove_prefix(1);
                return true;
            }
            return false;
        }

        if (pattern.front() == '\\')
        {
            // Match an escaped character.
            pattern.remove_prefix(1);
            if (pattern.front() == str.front())
            {
                pattern.remove_prefix(1);
                str.remove_prefix(1);
                return true;
            }
            return false;
        }

        if (pattern.front() == str.front() || pattern.front() == '?')
        {
            // Match a literal character or a '?'.
            pattern.remove_prefix(1);
            str.remove_prefix(1);
            return true;
        }

        return false;
    };

    while (!str.empty())
    {
        // Keep matching characters until we hit a "*" in the pattern.
        if (!pattern.empty() &&
            matchOne())
        {
            continue;
        }

        // There is no previous "*" to account for this mismatch.
        if (!patternStarSuffix)
        {
            return MatchType::MISMATCH;
        }

        // The new suffix would assume a delimiter matched by the "*"
        if (!isDoubleStar && starStr.front() == delimiter)
        {
            return MatchType::MISMATCH;
        }

        // Backtrack
        pattern = *patternStarSuffix;
        starStr.remove_prefix(1);
        str = starStr;
        bracketsIdx = bracketsStarMatchIdx;
    }

    // All bytes in str have been matched.
    // Return true if the rest of pattern is empty or contains only '*'s.
    if (pattern.find_first_not_of('*') == std::string::npos)
    {
        return MatchType::FULL;
    }
    return MatchType::PARTIAL;
}
} // (anon)

struct GlobPattern::Impl {
    std::string pattern;
    std::string prefix;
    llvm::SmallVector<SubGlobPattern, 1> subGlobs;
};

Expected<GlobPattern>
GlobPattern::
create(
    std::string_view const pattern,
    std::optional<std::size_t> maxSubGlobs)
{
    if (pattern.empty())
    {
        return {};
    }

    GlobPattern res;
    res.impl_ = std::make_unique<Impl>();

    // Store the original pattern.
    res.impl_->pattern = std::string(pattern);

    // Store the prefix that does not contain any metacharacter.
    std::size_t const prefixSize = pattern.find_first_of("?*[{\\");
    res.impl_->prefix = pattern.substr(0, prefixSize);
    if (prefixSize == std::string::npos)
    {
        // The pattern does not contain any metacharacter.
        return res;
    }

    // Parse the glob subpatterns.
    std::string_view const suffix = pattern.substr(prefixSize);
    MRDOCS_TRY(
        auto const subPatterns,
        parseBraceExpansions(suffix, maxSubGlobs));
    for (std::string_view subPattern: subPatterns)
    {
        MRDOCS_TRY(auto subGlob, SubGlobPattern::create(subPattern));
        res.impl_->subGlobs.push_back(std::move(subGlob));
    }
    return res;
}

GlobPattern::
~GlobPattern() = default;

GlobPattern::
GlobPattern() = default;

GlobPattern::
GlobPattern(GlobPattern const& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_) : nullptr)
{}

GlobPattern::
GlobPattern(GlobPattern&& other) noexcept
    : impl_(std::move(other.impl_))
{}

GlobPattern&
GlobPattern::
operator=(GlobPattern const& other)
{
    if (this != &other)
    {
        impl_ = other.impl_ ? std::make_unique<Impl>(*other.impl_) : nullptr;
    }
    return *this;
}

GlobPattern&
GlobPattern::
operator=(GlobPattern&& other) noexcept
{
    if (this != &other)
    {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool
GlobPattern::
match(std::string_view const str, char const delimiter) const
{
    if (!impl_)
    {
        return str.empty();
    }
    if (!str.starts_with(impl_->prefix))
    {
        return false;
    }
    std::string_view const suffix = str.substr(impl_->prefix.size());
    if (impl_->subGlobs.empty() && suffix.empty())
    {
        return true;
    }
    for (auto& subGlob: impl_->subGlobs)
    {
        if (auto m = subGlob.match(suffix, delimiter);
            m == SubGlobPattern::MatchType::FULL)
        {
            return true;
        }
    }
    return false;
}

bool
GlobPattern::
matchPatternPrefix(std::string_view const str, char const delimiter) const
{
    if (!impl_)
    {
        return str.empty();
    }
    if (str.size() < impl_->prefix.size())
    {
        return impl_->prefix.starts_with(str);
    }
    if (!str.starts_with(impl_->prefix))
    {
        return false;
    }
    std::string_view const suffix = str.substr(impl_->prefix.size());
    if (impl_->subGlobs.empty() && suffix.empty())
    {
        return true;
    }
    for (auto& subGlob: impl_->subGlobs)
    {
        if (auto m = subGlob.match(suffix, delimiter);
            m == SubGlobPattern::MatchType::FULL || m == SubGlobPattern::MatchType::PARTIAL)
        {
            return true;
        }
    }
    return false;
}

std::string_view
GlobPattern::
pattern() const
{
    if (!impl_)
    {
        return {};
    }
    return impl_->pattern;
}

} // clang::mrdocs

