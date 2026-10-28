//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_CONDITIONALSECTIONS_HPP
#define MRDOCS_LIB_AST_CONDITIONALSECTIONS_HPP

#include <clang/Basic/SourceLocation.h>
#include <map>
#include <vector>

namespace clang {
class ASTContext;
class Preprocessor;
} // clang

namespace mrdocs {

class Config;

/** The `\cond` ... `\endcond` regions of the files in a translation unit.

    Within one file, a region runs from a `\cond` (or `@cond`) marker to
    its matching `\endcond` (or `@endcond`) marker. Markers are matched as
    a stack across all the doc comments of the file, in source order, so a
    region can span several comments and nest inside another one.

    A region is enabled when it has a label, the label is listed in the
    `enabled-sections` option, and every enclosing region is enabled. All
    other regions are disabled, including the ones whose `\cond` has no
    label. A `\cond` with no matching `\endcond` extends to the end of its
    file.

    The regions of a file are computed on first use and cached, so a file
    that is never queried is never scanned. The markers are found with
    Clang's comment parser, so a marker inside a `\code` or `\verbatim`
    block is not a marker.
*/
class ConditionalSections
{
public:
    /** The way a marker fails to pair up.
    */
    enum class MarkerKind
    {
        /// A `\cond` with no matching `\endcond`.
        UnmatchedCond,
        /// An `\endcond` with no `\cond` to close.
        StrayEndcond
    };

    /** A marker that does not pair up.
    */
    struct Marker
    {
        /// How the marker fails to pair up.
        MarkerKind kind{ MarkerKind::UnmatchedCond };

        /// Where the marker is written.
        clang::SourceLocation loc{};
    };

    /** A half-open range of offsets within a file.
    */
    struct OffsetRange
    {
        /// The offset of the first character of the range.
        unsigned begin{ 0 };

        /// The offset one past the last character of the range.
        unsigned end{ 0 };
    };

    /** The regions of one file.
    */
    struct Regions
    {
        /// The outermost disabled regions, sorted and disjoint.
        std::vector<OffsetRange> disabled{};

        /// The markers that do not pair up, in source order.
        std::vector<Marker> malformed{};
    };

    /** Constructor.

        @param ctx The AST context that owns the comments.
        @param pp The preprocessor used to parse the comments.
        @param config The configuration that lists the enabled sections.
    */
    ConditionalSections(
        clang::ASTContext const& ctx,
        clang::Preprocessor const& pp,
        Config const& config) noexcept;

    /** Return the regions of a file.

        @param fid The file to scan.
    */
    Regions const&
    regionsOf(clang::FileID fid);

    /** Determine whether a location lies in a disabled region.

        The expansion location of `loc` is used, so a location produced
        by a macro expansion is looked up where the expansion happens.

        @param loc The location to look up.
    */
    bool
    isDisabled(clang::SourceLocation loc);

private:
    clang::ASTContext const& ctx_;
    clang::Preprocessor const& pp_;
    Config const& config_;
    // Node-based, so a reference returned by `regionsOf` survives later
    // insertions.
    std::map<clang::FileID, Regions> files_;
};

} // mrdocs

#endif // MRDOCS_LIB_AST_CONDITIONALSECTIONS_HPP
