//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_FUNCTIONCLASS_HPP
#define MRDOCS_API_METADATA_SYMBOL_FUNCTIONCLASS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs {

/** Function classifications */
enum class FunctionClass
{
    /// The function is a normal function.
    Normal = 0,
    /// The function is a constructor.
    Constructor,
    /// The function is a conversion operator.
    Conversion,
    /// The function is a destructor.
    Destructor
};

MRDOCS_DECL
dom::String
toString(FunctionClass kind) noexcept;

/** Return the FunctionClass from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    FunctionClass const kind)
{
    v = toString(kind);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_FUNCTIONCLASS_HPP
