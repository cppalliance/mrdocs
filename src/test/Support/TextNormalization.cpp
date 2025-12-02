//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "TextNormalization.hpp"
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>
#include <llvm/Support/Path.h>

namespace mrdocs::test_support {
namespace {

bool
isHorizontalSpace(char c)
{
    return c == ' ' || c == '\t';
}

void
normalizeNewlines(std::string& text)
{
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\r')
        {
            if (i + 1 < text.size() && text[i + 1] == '\n')
            {
                ++i;
            }
            normalized.push_back('\n');
        }
        else
        {
            normalized.push_back(text[i]);
        }
    }
    text.swap(normalized);
}

void
rstripEachLine(std::string& text)
{
    std::string trimmed;
    trimmed.reserve(text.size());
    std::size_t lineStart = 0;
    for (std::size_t i = 0; i <= text.size(); ++i)
    {
        if (i == text.size() || text[i] == '\n')
        {
            std::size_t lineEnd = i;
            while (lineEnd > lineStart &&
                (text[lineEnd - 1] == ' ' ||
                 text[lineEnd - 1] == '\t' ||
                 text[lineEnd - 1] == '\r'))
            {
                --lineEnd;
            }
            trimmed.append(text.data() + lineStart, lineEnd - lineStart);
            if (i != text.size())
            {
                trimmed.push_back('\n');
            }
            lineStart = i + 1;
        }
    }
    text.swap(trimmed);
}

void
collapseBlankLines(std::string& text, std::size_t maxBlankLines)
{
    if (text.empty())
        return;

    std::string collapsed;
    collapsed.reserve(text.size());
    std::size_t blankCount = 0;
    std::size_t pos = 0;
    while (pos < text.size())
    {
        auto nextNewline = text.find('\n', pos);
        bool hasNewline = nextNewline != std::string::npos;
        std::size_t lineLength =
            (hasNewline ? nextNewline : text.size()) - pos;
        std::string_view line(text.data() + pos, lineLength);
        bool isBlank = line.empty();

        if (!isBlank || blankCount < maxBlankLines)
        {
            collapsed.append(line);
            if (hasNewline)
            {
                collapsed.push_back('\n');
            }
        }

        blankCount = isBlank ? blankCount + 1 : 0;
        if (!hasNewline)
            break;
        pos = nextNewline + 1;
    }

    text.swap(collapsed);
}

std::string
collapseSpacesOutsideVerbatim(
    std::string_view text,
    std::initializer_list<llvm::StringRef> verbatimTags)
{
    std::vector<std::string> verbatim;
    verbatim.reserve(verbatimTags.size());
    for (auto const tag : verbatimTags)
    {
        verbatim.emplace_back(tag.lower());
    }

    std::vector<std::string> verbatimStack;
    std::string out;
    out.reserve(text.size());

    bool previousSpace = false;
    std::size_t i = 0;
    while (i < text.size())
    {
        if (text[i] == '<')
        {
            auto close = text.find('>', i);
            if (close == std::string::npos)
            {
                out.append(text.substr(i));
                break;
            }

            llvm::StringRef tag(text.data() + i + 1, close - i - 1);
            tag = tag.ltrim();
            bool isClosing = tag.consume_front("/");
            tag = tag.ltrim();
            llvm::StringRef tagBody = tag.rtrim();
            bool selfClosing = tagBody.ends_with("/");
            llvm::StringRef name = tag.take_while([](char c) {
                return std::isalnum(static_cast<unsigned char>(c)) ||
                    c == '-' || c == ':';
            });
            std::string lowerName = name.lower();

            if (isClosing)
            {
                if (!verbatimStack.empty() &&
                    verbatimStack.back() == lowerName)
                {
                    verbatimStack.pop_back();
                }
            }
            else
            {
                bool isVerbatim = std::find(
                    verbatim.begin(), verbatim.end(), lowerName) != verbatim.end();
                if (isVerbatim && !selfClosing)
                {
                    verbatimStack.push_back(lowerName);
                }
            }

            out.append(text.substr(i, close - i + 1));
            previousSpace = false;
            i = close + 1;
            continue;
        }

        char c = text[i];
        if (verbatimStack.empty() && isHorizontalSpace(c))
        {
            if (!previousSpace)
            {
                out.push_back(' ');
            }
            previousSpace = true;
            ++i;
            continue;
        }

        previousSpace = false;
        out.push_back(c);
        ++i;
    }

    return out;
}

} // namespace

OutputFormat
guessOutputFormat(llvm::StringRef pathOrExtension)
{
    llvm::StringRef ext = llvm::sys::path::extension(pathOrExtension);
    if (ext.empty())
        ext = pathOrExtension;
    ext = ext.ltrim(".");
    auto lower = ext.lower();
    llvm::StringRef extLower(lower);

    if (extLower == "html" || extLower == "htm")
        return OutputFormat::html;
    if (extLower == "adoc" || extLower == "asciidoc")
        return OutputFormat::adoc;
    if (extLower == "xml")
        return OutputFormat::xml;
    return OutputFormat::other;
}

std::string
normalizeForComparison(std::string_view text, OutputFormat format)
{
    std::string normalized(text);
    normalizeNewlines(normalized);

    switch (format)
    {
    case OutputFormat::html:
        normalized = collapseSpacesOutsideVerbatim(
            normalized, { "pre", "code", "textarea" });
        rstripEachLine(normalized);
        break;

    case OutputFormat::xml:
        rstripEachLine(normalized);
        collapseBlankLines(normalized, 1);
        break;

    case OutputFormat::adoc:
        rstripEachLine(normalized);
        collapseBlankLines(normalized, 1);
        break;

    case OutputFormat::other:
        rstripEachLine(normalized);
        break;
    }

    return normalized;
}

std::string
normalizeForComparison(std::string_view text, llvm::StringRef pathOrExtension)
{
    return normalizeForComparison(text, guessOutputFormat(pathOrExtension));
}

} // namespace mrdocs::test_support
