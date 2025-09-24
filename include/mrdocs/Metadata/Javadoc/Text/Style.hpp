//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_JAVADOC_TEXT_STYLE_HPP
#define MRDOCS_API_METADATA_JAVADOC_TEXT_STYLE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace clang::mrdocs::doc {

/** The text style.
*/
enum class Style
{
    /// No style
    none = 1, // needed by bitstream
    /// Monospaced text
    mono,
    /// Bold text
    bold,
    /// Italic text
    italic
};

/** Return the name of the @ref Style as a string.

    @param kind The style kind.
    @return The string representation of the style.
 */
MRDOCS_DECL
dom::String
toString(Style kind) noexcept;

/** Return the @ref Style from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Style const kind)
{
    v = toString(kind);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_TEXT_STYLE_HPP
