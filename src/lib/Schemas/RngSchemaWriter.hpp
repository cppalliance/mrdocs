//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SCHEMAS_RNGSCHEMAWRITER_HPP
#define MRDOCS_LIB_SCHEMAS_RNGSCHEMAWRITER_HPP

#include <string>

namespace mrdocs::schema {

/** Build the RELAX NG (XML syntax) schema for MrDocs XML output.

    The returned text is a complete `<grammar>` document in the
    RELAX NG XML namespace. It is consumed directly by validators
    such as `xmllint --relaxng` — no `trang` conversion is needed.

    The grammar is driven by reflection over the described
    metadata types and mirrors what `XMLWriter` actually emits.

    @return The full schema text, ready to be written to
    `mrdocs.rng`.
*/
std::string
buildRngSchema();

} // mrdocs::schema

#endif // MRDOCS_LIB_SCHEMAS_RNGSCHEMAWRITER_HPP
