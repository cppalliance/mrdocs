//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_IMAGEINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_IMAGEINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** An image.

    Syntax:

    @code
    @image html path/to/img "alt text"
    @endcode

    or

    @code
    ![Alt text](image_url "Optional title text")
    @endcode
*/
struct ImageInline final
    : InlineCommonBase<InlineKind::Image>
    , InlineContainer
{
    /** Image source URL or path.
    */
    std::string src;
    /** Alternate text when the image cannot be shown.
    */
    std::string alt;

    /** Construct an empty image.
    */
    ImageInline() = default;

    /** Construct an image with a source and alternate text.

        @param src Image source URL or path.
        @param alt Alternate text when the image cannot be shown.
    */
    ImageInline(std::string_view src, std::string_view alt)
        : src(src)
        , alt(alt)
    {}

};

MRDOCS_DESCRIBE_STRUCT(
    ImageInline,
    (InlineCommonBase<InlineKind::Image>, InlineContainer),
    (src, alt)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_IMAGEINLINE_HPP
