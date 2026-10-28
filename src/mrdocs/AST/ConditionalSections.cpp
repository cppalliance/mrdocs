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
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs {

namespace {

// A `\cond` or `\endcond` marker, as written in a doc comment.
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

// Find the markers in the doc comments of one file, in source order.
class MarkerCollector
{
    clang::ASTContext const& ctx_;
    clang::Preprocessor const& pp_;
    std::vector<ParsedMarker> markers_;

    unsigned
    offsetOf(clang::SourceLocation loc) const
    {
        return ctx_.getSourceManager().getDecomposedExpansionLoc(loc).second;
    }

    void
    addMarker(clang::comments::VerbatimLineComment const* C, bool isEnd)
    {
        markers_.push_back({
            .isEnd = isEnd,
            .loc = C->getBeginLoc(),
            .begin = offsetOf(C->getBeginLoc()),
            .end = offsetOf(C->getEndLoc()),
            // The label is the first word after the command. Any further
            // text on the line is ignored.
            .label = llvm::getToken(C->getText()).first.str() });
    }

    void
    visit(clang::comments::VerbatimLineComment const* C)
    {
        llvm::StringRef const name =
            C->getCommandName(ctx_.getCommentCommandTraits());
        if (name == "cond" || name == "endcond")
        {
            addMarker(C, name == "endcond");
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

    // The translation unit stands in for the `Decl` the comment would be
    // attached to: the parser needs a valid one, but the markers do not
    // depend on it.
    void
    parse(clang::RawComment const& rc)
    {
        clang::comments::FullComment const* FC =
            rc.parse(ctx_, &pp_, ctx_.getTranslationUnitDecl());
        if (FC)
        {
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

    // A `\cond` left open extends to the end of the file.
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

bool
containsOffset(
    std::vector<ConditionalSections::OffsetRange> const& ranges,
    unsigned offset)
{
    std::vector<ConditionalSections::OffsetRange>::const_iterator next =
        std::ranges::upper_bound(
            ranges, offset, {}, &ConditionalSections::OffsetRange::begin);
    return next != ranges.begin() && offset < std::prev(next)->end;
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

bool
ConditionalSections::
isDisabled(clang::SourceLocation loc)
{
    bool result = false;
    if (loc.isValid())
    {
        std::pair<clang::FileID, unsigned> const where =
            ctx_.getSourceManager().getDecomposedExpansionLoc(loc);
        if (where.first.isValid())
        {
            result = containsOffset(
                regionsOf(where.first).disabled, where.second);
        }
    }
    return result;
}

} // mrdocs
