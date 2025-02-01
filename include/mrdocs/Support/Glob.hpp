//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_GLOBPATTERN_HPP
#define MRDOCS_API_SUPPORT_GLOBPATTERN_HPP

#include <mrdocs/Support/Error.hpp>
#include <memory>
#include <optional>
#include <string_view>

namespace clang::mrdocs {

/** A glob pattern matcher

    @li "*" matches all characters except delimiters.
    @li "**" matches all characters
    @li "?" matches any single character.
    @li "[<chars>]" matches one character in the bracket.
    @li "[<char>-<char>]" matches one character in the bracket range.
    @li "[^<chars>]" or "[!<chars>]" matches one character not in the bracket.
    @li "{<glob>,...}" matches one of the globs in the list.
    @li "\" escapes the next character so it is treated as a literal.

    Nested brace expansions "{<glob>,"{<glob>,...}",...}" are not supported.
 */
class MRDOCS_DECL GlobPattern {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    /** Constructs a GlobPattern with the given pattern.

        @param pattern The glob pattern to use for matching.
     */
    static
    Expected<GlobPattern>
    create(std::string_view pattern, std::optional<std::size_t> maxSubGlobs);

    static
    Expected<GlobPattern>
    create(std::string_view pattern)
    {
        return create(pattern, std::nullopt);
    }

    /** Destructor */
    ~GlobPattern();

    /** Construct an empty GlobPattern.

        An empty GlobPattern will never match any string.
     */
    GlobPattern();

    /** Copy constructor */
    GlobPattern(GlobPattern const& other);

    /** Move constructor */
    GlobPattern(GlobPattern&& other) noexcept;

    /** Copy assignment */
    GlobPattern&
    operator=(GlobPattern const& other);

    /** Move assignment */
    GlobPattern&
    operator=(GlobPattern&& other) noexcept;

    /** Matches the given string against the glob pattern.

        @param str The string to match against the pattern.
        @return true if the string matches the pattern, false otherwise.
     */
    bool
    match(std::string_view str, char delimiter) const;

    /** Matches the start of a given string against the glob pattern.

        This function determines if the given string with
        the specified `prefix` can potentially match
        the glob pattern.

        If the string matches the start of the pattern without failure, even
        if there are characters left in the string or the pattern, the
        function returns true.

        @param prefix The string to match against the pattern.
        @return true if the string prefix matches the pattern, false otherwise.
     */
    bool
    matchPatternPrefix(std::string_view prefix, char delimiter) const;

    /** Checks if the glob pattern is a literal string.

        This function determines if the glob pattern does not contain
        any special characters. In other words, it matches a single string.

        @return true if the glob pattern is a literal string, false otherwise.
     */
    bool
    isLiteral() const;

    /** Returns the glob pattern.

        @return The glob pattern as a string view.
     */
    std::string_view
    pattern() const;
};

/** A glob pattern matcher for paths

    A glob pattern matcher where "*" does not match path separators.
    The pattern "**" can be used to match any number of path separators.
 */
class MRDOCS_DECL PathGlobPattern {
    GlobPattern glob_;
public:
    /** Constructs a PathGlobPattern with the given pattern.

        @param pattern The glob pattern to use for matching.
        @param maxSubGlobs The maximum number of subpatterns allowed.
     */
    static
    Expected<PathGlobPattern>
    create(
        std::string_view const pattern,
        std::optional<std::size_t> maxSubGlobs)
    {
        MRDOCS_TRY(auto glob, GlobPattern::create(pattern, maxSubGlobs));
        return PathGlobPattern{std::move(glob)};
    }

    /** Constructs a PathGlobPattern with the given pattern.

        @param pattern The glob pattern to use for matching.
     */
    static
    Expected<PathGlobPattern>
    create(std::string_view const pattern)
    {
        MRDOCS_TRY(auto glob, GlobPattern::create(pattern));
        return PathGlobPattern{std::move(glob)};
    }

    /** Construct an empty PathGlobPattern.

        An empty PathGlobPattern will never match any string.
     */
    PathGlobPattern() = default;

    /** Construct an empty PathGlobPattern.

        An empty PathGlobPattern will never match any string.
     */
    explicit
    PathGlobPattern(GlobPattern glob)
        : glob_{std::move(glob)}
    {};

    /** Matches the given string against the glob pattern.

        @param str The string to match against the pattern.
        @return true if the string matches the pattern, false otherwise.
     */
    bool
    match(std::string_view const str) const
    {
        return glob_.match(str, '/');
    }

    /** Matches the start of a given string against the glob pattern.

        This function determines if the given string with
        the specified `prefix` can potentially match
        the glob pattern.

        If the string matches the start of the pattern without failure, even
        if there are characters left in the string or the pattern, the
        function returns true.

        @param prefix The string to match against the pattern.
        @return true if the string prefix matches the pattern, false otherwise.
     */
    bool
    matchPatternPrefix(std::string_view prefix) const
    {
        return glob_.matchPatternPrefix(prefix, '/');
    }

    /** Checks if the glob pattern is a literal string.

        This function determines if the glob pattern does not contain
        any special characters. In other words, it matches a single string.

        @return true if the glob pattern is a literal string, false otherwise.
     */
    bool
    isLiteral() const
    {
        return glob_.isLiteral();
    }

    /** Returns the glob pattern.

        @return The glob pattern as a string view.
     */
    std::string_view
    pattern() const
    {
        return glob_.pattern();
    }
};

/** A glob pattern matcher for C++ symbols

    A glob pattern matcher where "*" does not match "::".
    The pattern "**" can be used to match any number of "::".
 */
class MRDOCS_DECL SymbolGlobPattern {
    GlobPattern glob_;
public:
    /** Constructs a SymbolGlobPattern with the given pattern.

        @param pattern The glob pattern to use for matching.
        @param maxSubGlobs The maximum number of subpatterns allowed.
     */
    static
    Expected<SymbolGlobPattern>
    create(
        std::string_view const pattern,
        std::optional<std::size_t> maxSubGlobs)
    {
        MRDOCS_TRY(auto glob, GlobPattern::create(pattern, maxSubGlobs));
        return SymbolGlobPattern{std::move(glob)};
    }

    /** Constructs a SymbolGlobPattern with the given pattern.

        @param pattern The glob pattern to use for matching.
     */
    static
    Expected<SymbolGlobPattern>
    create(std::string_view const pattern)
    {
        MRDOCS_TRY(auto glob, GlobPattern::create(pattern));
        return SymbolGlobPattern{std::move(glob)};
    }

    /** Construct an empty SymbolGlobPattern.

        An empty SymbolGlobPattern will never match any string.
     */
    SymbolGlobPattern() = default;

    /** Construct an empty SymbolGlobPattern.

        An empty SymbolGlobPattern will never match any string.
     */
    explicit
    SymbolGlobPattern(GlobPattern glob)
        : glob_{std::move(glob)}
    {};

    /** Matches the given string against the glob pattern.

        @param str The string to match against the pattern.
        @return true if the string matches the pattern, false otherwise.
     */
    bool
    match(std::string_view const str) const
    {
        return glob_.match(str, ':');
    }

    /** Matches the start of a given string against the glob pattern.

        This function determines if the given string with
        the specified `prefix` can potentially match
        the glob pattern.

        If the string matches the start of the pattern without failure, even
        if there are characters left in the string or the pattern, the
        function returns true.

        @param prefix The string to match against the pattern.
        @return true if the string prefix matches the pattern, false otherwise.
     */
    bool
    matchPatternPrefix(std::string_view prefix) const
    {
        return glob_.matchPatternPrefix(prefix, ':');
    }

    /** Checks if the glob pattern is a literal string.

        This function determines if the glob pattern does not contain
        any special characters. In other words, it matches a single string.

        @return true if the glob pattern is a literal string, false otherwise.
     */
    bool
    isLiteral() const
    {
        return glob_.isLiteral();
    }

    /** Returns the glob pattern.

        @return The glob pattern as a string view.
     */
    std::string_view
    pattern() const
    {
        return glob_.pattern();
    }
};

} // clang::mrdocs

#endif // MRDOCS_API_SUPPORT_GLOBPATTERN_HPP