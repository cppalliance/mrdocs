//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_EXTRACTIONMODE_HPP
#define MRDOCS_API_METADATA_EXTRACTIONMODE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <string_view>

namespace clang::mrdocs {

/** Determine why a symbol is extracted

    The enum constants are ordered by specificity, with
    the least specific at the beginning and the most
    specific at the end.

 */
enum class ExtractionMode
{
    /// We're extracting the current symbol even though it
    /// doesn't pass all filters because it's a direct dependency
    /// of a symbol that does pass all filters and needs
    /// information about it (e.g.: base classes outside the filters)
    Dependency,

    /// We're extracting the current symbol because it passes
    /// all filters
    Regular,

    /// We're extracting the current symbol because it passes
    /// all filters, but we should also tag it as see-below
    /// because it passes one of the see-below filters
    SeeBelow,

    /// We're extracting the current symbol because it passes
    /// all filters, but we should also tag it as
    /// implementation-defined because one of its parents
    /// matches the implementation-defined filter
    ImplementationDefined,
};

/** Return the name of the InfoKind as a string.
 */
constexpr
std::string_view
toString(ExtractionMode kind) noexcept
{
    switch(kind)
    {
    case ExtractionMode::Dependency:
        return "dependency";
    case ExtractionMode::Regular:
        return "regular";
    case ExtractionMode::SeeBelow:
        return "see-below";
    case ExtractionMode::ImplementationDefined:
        return "implementation-defined";
    }
    MRDOCS_UNREACHABLE();
}

/** Return the InfoKind from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ExtractionMode kind)
{
    v = toString(kind);
}

/** Compare ExtractionModes and returns the least specific

    This function returns the least specific of the two
    ExtractionModes in terms of number of filters passed.
 */
constexpr
ExtractionMode
leastSpecific(ExtractionMode const a, ExtractionMode const b) noexcept
{
    using IT = std::underlying_type_t<ExtractionMode>;
    return static_cast<ExtractionMode>(
            std::min(static_cast<IT>(a), static_cast<IT>(b)));
}

/** Compare ExtractionModes and returns the most specific

    This function returns the most specific of the two
    ExtractionModes in terms of number of filters passed.
 */
constexpr
ExtractionMode
mostSpecific(ExtractionMode const a, ExtractionMode const b) noexcept
{
    using IT = std::underlying_type_t<ExtractionMode>;
    return static_cast<ExtractionMode>(
            std::max(static_cast<IT>(a), static_cast<IT>(b)));
}

} // clang::mrdocs

#endif
