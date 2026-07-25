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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONKIND_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs::doc {

/** An admonishment style.
*/
enum class AdmonitionKind
{
    /// No admonishment
    none = 1, // needed by bitstream
    /// A general note
    note,
    /// A tip to the reader
    tip,
    /// Something important
    important,
    /// A caution admonishment
    caution,
    /// A warning admonishment
    warning
};

MRDOCS_DESCRIBE_ENUM(
    AdmonitionKind,
    none, note, tip, important, caution, warning)
MRDOCS_DESCRIBE_ENUM_UNDEFINED(AdmonitionKind, none)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONKIND_HPP
