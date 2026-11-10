//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GENERATORS_BUILTINGENERATORS_HPP
#define MRDOCS_LIB_GENERATORS_BUILTINGENERATORS_HPP

#include <mrdocs/Support/Error/Expected.hpp>

namespace mrdocs {

class GeneratorRegistry;

/** Add the generators that ship with MrDocs to a registry.

    The registry calls this once, when it is constructed: `adoc`, `xml`,
    `json`, `html` and `noop`, in that order. The ids are distinct, so the
    call fails only if `registry` already holds one of them.

    @param registry The registry to add the generators to.
    @return The error, if a generator could not be added.
*/
Expected<void>
registerBuiltinGenerators(GeneratorRegistry& registry);

} // mrdocs

#endif
