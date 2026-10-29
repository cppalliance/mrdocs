//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ConditionalSections.hpp"
#include <mrdocs/Config.hpp>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Comment.h>
#include <clang/AST/CommentCommandTraits.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/StringExtras.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs {

namespace {

// A `cond` or `endcond` marker, as written in a doc comment.
struct ParsedMarker
{
    bool isEnd{ false };
    clang::SourceLocation loc{};
    unsigned begin{ 0 };
    unsigned end{ 0 };
    std::string label{};
};

// The spellings a doc comment must contain to hold a marker. Used to skip
// parsing the comments that cannot.
constexpr std::array<std::string_view, 4> markerSpellings = {
    "\\cond", "@cond", "\\endcond", "@endcond"
};

bool
mayHoldMarker(clang::RawComment const& rc, clang::SourceManager const& sm)
{
    std::string_view const text = rc.getRawText(sm);
    return std::ranges::any_of(
        markerSpellings,
        [&](std::string_view spelling)
        {
            return text.contains(spelling);
        });
}

// Return the length of the run of backticks that starts at `pos`.
std::size_t
runLength(llvm::StringRef line, std::size_t pos)
{
    std::size_t const end = line.find_first_not_of('`', pos);
    return (end == llvm::StringRef::npos ? line.size() : end) - pos;
}

// Determine whether the backtick at `pos` is escaped, that is, preceded by an
// odd number of backslashes.
bool
isEscaped(llvm::StringRef line, std::size_t pos)
{
    std::size_t slashes = 0;
    while (slashes < pos && line[pos - slashes - 1] == '\\')
    {
        ++slashes;
    }
    return slashes % 2 == 1;
}

// Find the first run of exactly `length` backticks at or after `pos`. A
// backslash does not escape a backtick inside a code span.
std::size_t
findClosingRun(llvm::StringRef line, std::size_t pos, std::size_t length)
{
    std::size_t at = line.find('`', pos);
    while (at != llvm::StringRef::npos && runLength(line, at) != length)
    {
        at = line.find('`', at + runLength(line, at));
    }
    return at;
}

// A Markdown code span on one line: the offset of its opening backtick run
// and the offset one past its closing one.
struct CodeSpan
{
    std::size_t begin{ 0 };
    std::size_t end{ 0 };
};

// Find the first code span that starts at or after `pos`. A run of backticks
// opens a span only if a later run of the same length closes it; otherwise it
// is literal text. An escaped backtick does not open a span.
std::optional<CodeSpan>
findCodeSpan(llvm::StringRef line, std::size_t pos)
{
    std::optional<CodeSpan> result;
    std::size_t at = line.find('`', pos);
    while (!result && at != llvm::StringRef::npos)
    {
        bool const escaped = isEscaped(line, at);
        std::size_t const run = escaped ? 1 : runLength(line, at);
        std::size_t const close = escaped
            ? llvm::StringRef::npos
            : findClosingRun(line, at + run, run);
        if (close != llvm::StringRef::npos)
        {
            result = CodeSpan{ at, close + run };
        }
        else
        {
            at = line.find('`', at + run);
        }
    }
    return result;
}

// Determine whether the offset `at` of `line` lies inside one of its code
// spans.
bool
spanCovers(llvm::StringRef line, std::size_t at)
{
    bool result = false;
    std::optional<CodeSpan> span = findCodeSpan(line, 0);
    while (span && span->begin < at && !result)
    {
        result = at < span->end;
        span = findCodeSpan(line, span->end);
    }
    return result;
}

// Determine whether a command is inside a Markdown code span, where Doxygen
// does not interpret it. Spans do not cross lines, and the line is cut to the
// doc comment, so that code before or after it is not scanned.
bool
isInCodeSpan(
    clang::SourceManager const& sm,
    clang::SourceLocation loc,
    ConditionalSections::OffsetRange const& comment)
{
    std::pair<clang::FileID, unsigned> const where =
        sm.getDecomposedExpansionLoc(loc);
    llvm::StringRef const buffer = sm.getBufferData(where.first);
    std::size_t const start = std::max<std::size_t>(
        buffer.substr(0, where.second).find_last_of("\r\n") + 1,
        comment.begin);
    std::size_t const end = std::min<std::size_t>(
        buffer.find_first_of("\r\n", where.second), comment.end);
    return spanCovers(buffer.slice(start, end), where.second - start);
}

// A `cond` or `endcond` command that is not inside a code span.
bool
isMarkerCommand(
    clang::ASTContext const& ctx,
    clang::comments::VerbatimLineComment const* C,
    ConditionalSections::OffsetRange const& comment)
{
    llvm::StringRef const name =
        C->getCommandName(ctx.getCommentCommandTraits());
    return (name == "cond" || name == "endcond")
        && !isInCodeSpan(ctx.getSourceManager(), C->getBeginLoc(), comment);
}

// The offset of `loc` within the file it expands in.
unsigned
offsetOf(clang::SourceManager const& sm, clang::SourceLocation loc)
{
    return sm.getDecomposedExpansionLoc(loc).second;
}

// The offsets of a doc comment within its file.
ConditionalSections::OffsetRange
rangeOf(clang::RawComment const& rc, clang::SourceManager const& sm)
{
    return { offsetOf(sm, rc.getBeginLoc()), offsetOf(sm, rc.getEndLoc()) };
}

// Parse a doc comment. The translation unit stands in for the `Decl` the
// comment would be attached to: the parser needs a valid one, but the block
// structure does not depend on it.
clang::comments::FullComment const*
parseBlocks(
    clang::ASTContext const& ctx,
    clang::Preprocessor const& pp,
    clang::RawComment const& rc)
{
    return rc.parse(ctx, &pp, ctx.getTranslationUnitDecl());
}

// Find the markers in the doc comments of one file, in source order.
class MarkerCollector
{
    clang::ASTContext const& ctx_;
    clang::Preprocessor const& pp_;
    std::vector<ParsedMarker> markers_;
    // The range of the doc comment being parsed.
    ConditionalSections::OffsetRange comment_{};

    void
    addMarker(clang::comments::VerbatimLineComment const* C)
    {
        clang::SourceManager const& sm = ctx_.getSourceManager();
        markers_.push_back({
            .isEnd = C->getCommandName(ctx_.getCommentCommandTraits())
                == "endcond",
            .loc = C->getBeginLoc(),
            .begin = offsetOf(sm, C->getBeginLoc()),
            .end = offsetOf(sm, C->getEndLoc()),
            // The label is the first word after the command. Any further
            // text on the line is ignored.
            .label = llvm::getToken(C->getText()).first.str() });
    }

    void
    visit(clang::comments::VerbatimLineComment const* C)
    {
        if (isMarkerCommand(ctx_, C, comment_))
        {
            addMarker(C);
        }
    }

    void
    walk(clang::comments::Comment const* C)
    {
        if (C->getCommentKind()
            == clang::comments::CommentKind::VerbatimLineComment)
        {
            visit(static_cast<clang::comments::VerbatimLineComment const*>(C));
        }
        for (clang::comments::Comment const* child :
             llvm::make_range(C->child_begin(), C->child_end()))
        {
            walk(child);
        }
    }

    void
    parse(clang::RawComment const& rc)
    {
        clang::comments::FullComment const* FC = parseBlocks(ctx_, pp_, rc);
        if (FC)
        {
            comment_ = rangeOf(rc, ctx_.getSourceManager());
            walk(FC);
        }
    }

public:
    MarkerCollector(
        clang::ASTContext const& ctx,
        clang::Preprocessor const& pp)
        : ctx_(ctx)
        , pp_(pp)
    {}

    std::vector<ParsedMarker>
    collect(clang::FileID fid) &&
    {
        std::map<unsigned, clang::RawComment*> const* comments =
            ctx_.Comments.getCommentsInFile(fid);
        if (comments)
        {
            for (auto const& [offset, rc] : *comments)
            {
                if (mayHoldMarker(*rc, ctx_.getSourceManager()))
                {
                    parse(*rc);
                }
            }
        }
        return std::move(markers_);
    }
};

// Pair the markers of one file into regions.
class RegionMatcher
{
    struct Frame
    {
        unsigned begin{ 0 };
        clang::SourceLocation loc{};
        bool enabled{ false };
    };

    std::vector<std::string> const& enabledSections_;
    std::vector<Frame> stack_;
    ConditionalSections::Regions regions_;

    bool
    parentEnabled() const
    {
        return stack_.empty() || stack_.back().enabled;
    }

    bool
    labelEnabled(std::string const& label) const
    {
        return !label.empty()
            && std::ranges::find(enabledSections_, label)
                != enabledSections_.end();
    }

    void
    open(ParsedMarker const& marker)
    {
        bool const enabled = parentEnabled() && labelEnabled(marker.label);
        stack_.push_back({ marker.begin, marker.loc, enabled });
    }

    // Only the outermost disabled region is recorded: one nested in a
    // disabled region is already covered.
    void
    pop(unsigned end)
    {
        Frame const frame = stack_.back();
        stack_.pop_back();
        if (!frame.enabled && parentEnabled())
        {
            regions_.disabled.push_back({ frame.begin, end });
        }
    }

    void
    close(ParsedMarker const& marker)
    {
        if (stack_.empty())
        {
            regions_.malformed.push_back({
                ConditionalSections::MarkerKind::StrayEndcond,
                marker.loc });
        }
        else
        {
            pop(marker.end);
        }
    }

    // A `cond` left open extends to the end of the file.
    void
    closeAll()
    {
        while (!stack_.empty())
        {
            regions_.malformed.push_back({
                ConditionalSections::MarkerKind::UnmatchedCond,
                stack_.back().loc });
            pop(std::numeric_limits<unsigned>::max());
        }
        std::ranges::sort(
            regions_.malformed, {}, &ConditionalSections::Marker::loc);
    }

public:
    explicit
    RegionMatcher(std::vector<std::string> const& enabledSections)
        : enabledSections_(enabledSections)
    {}

    ConditionalSections::Regions
    match(std::vector<ParsedMarker> const& markers) &&
    {
        for (ParsedMarker const& marker : markers)
        {
            regions_.markerOffsets.push_back(marker.begin);
            if (marker.isEnd)
            {
                close(marker);
            }
            else
            {
                open(marker);
            }
        }
        closeAll();
        return std::move(regions_);
    }
};

// Determine whether `[begin, end)` overlaps any of the sorted, disjoint ranges.
bool
overlapsRange(
    std::vector<ConditionalSections::OffsetRange> const& ranges,
    unsigned begin,
    unsigned end)
{
    // The first range that ends after `begin` is the only candidate.
    std::vector<ConditionalSections::OffsetRange>::const_iterator next =
        std::ranges::upper_bound(
            ranges, begin, {}, &ConditionalSections::OffsetRange::end);
    return next != ranges.end() && next->begin < end;
}

// Determine whether any of the sorted `offsets` lies in `[begin, end)`.
bool
holdsOffset(std::vector<unsigned> const& offsets, unsigned begin, unsigned end)
{
    std::vector<unsigned>::const_iterator next =
        std::ranges::lower_bound(offsets, begin);
    return next != offsets.end() && *next < end;
}

// Whether a block of a doc comment renders anything when it is enabled. A
// verbatim line command, such as the `cond` and `endcond` markers, never
// does, and neither does the whitespace Clang leaves in a paragraph of its
// own around a command.
bool
rendersContent(clang::comments::Comment const* block)
{
    bool result = block->getCommentKind()
        != clang::comments::CommentKind::VerbatimLineComment;
    if (block->getCommentKind()
        == clang::comments::CommentKind::ParagraphComment)
    {
        result = !static_cast<clang::comments::ParagraphComment const*>(
            block)->isWhitespace();
    }
    return result;
}

} // (anon)

ConditionalSections::
ConditionalSections(
    clang::ASTContext const& ctx,
    clang::Preprocessor const& pp,
    Config const& config) noexcept
    : ctx_(ctx)
    , pp_(pp)
    , config_(config)
{}

ConditionalSections::Regions const&
ConditionalSections::
regionsOf(clang::FileID fid)
{
    std::map<clang::FileID, Regions>::iterator it = files_.find(fid);
    if (it == files_.end())
    {
        std::vector<ParsedMarker> markers =
            MarkerCollector(ctx_, pp_).collect(fid);
        it = files_.emplace(
            fid,
            RegionMatcher(config_.enabledSections).match(markers)).first;
    }
    return it->second;
}

std::pair<ConditionalSections::Regions const*, unsigned>
ConditionalSections::
locate(clang::SourceLocation loc)
{
    Regions const* regions = nullptr;
    unsigned offset = 0;
    if (loc.isValid())
    {
        std::pair<clang::FileID, unsigned> const where =
            ctx_.getSourceManager().getDecomposedExpansionLoc(loc);
        if (where.first.isValid())
        {
            regions = &regionsOf(where.first);
            offset = where.second;
        }
    }
    return { regions, offset };
}

std::map<clang::FileID, ConditionalSections::Regions> const&
ConditionalSections::
computedFiles() const noexcept
{
    return files_;
}

bool
ConditionalSections::
isDisabled(clang::SourceLocation loc)
{
    std::pair<Regions const*, unsigned> const where = locate(loc);
    return where.first
        && overlapsRange(where.first->disabled, where.second, where.second + 1);
}

bool
ConditionalSections::
reachesSection(clang::RawComment const& rc)
{
    Regions const* regions = locate(rc.getBeginLoc()).first;
    OffsetRange const range = rangeOf(rc, ctx_.getSourceManager());
    return regions
        && (overlapsRange(regions->disabled, range.begin, range.end)
            || holdsOffset(regions->markerOffsets, range.begin, range.end));
}

bool
ConditionalSections::
hasRenderedBlock(clang::comments::FullComment const& FC)
{
    return std::ranges::any_of(
        FC.getBlocks(),
        [&](clang::comments::Comment const* block)
        {
            return rendersContent(block)
                && !isDisabled(block->getBeginLoc());
        });
}

bool
ConditionalSections::
hidesAll(clang::RawComment const& rc)
{
    bool result = false;
    if (reachesSection(rc))
    {
        clang::comments::FullComment const* FC =
            parseBlocks(ctx_, pp_, rc);
        result = FC && !FC->getBlocks().empty() && !hasRenderedBlock(*FC);
    }
    return result;
}

} // mrdocs
