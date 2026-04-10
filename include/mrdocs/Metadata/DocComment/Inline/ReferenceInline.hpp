//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_REFERENCEINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_REFERENCEINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A reference to a symbol.

    Syntax:

    @code
    @ref target "label"
    @endcode
*/
struct ReferenceInline
    : InlineCommonBase<InlineKind::Reference>
{
    /** Display text of the reference.
    */
    std::string literal;
    /** Symbol being referenced.
    */
    SymbolID id = SymbolID::invalid;

    /** Construct a reference with optional display text.
    */
    explicit ReferenceInline(std::string str = {}) noexcept
        : literal(std::move(str))
    {}

};

MRDOCS_DESCRIBE_STRUCT(
    ReferenceInline,
    (InlineCommonBase<InlineKind::Reference>),
    (literal, id)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_REFERENCEINLINE_HPP
