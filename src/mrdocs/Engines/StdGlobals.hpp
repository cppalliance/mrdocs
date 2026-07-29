//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_ENGINES_STDGLOBALS_HPP
#define MRDOCS_LIB_ENGINES_STDGLOBALS_HPP

#include <mrdocs/Engines/JavaScript.hpp>

namespace mrdocs::js {

/** Install MrDocs's standard set of script-side globals on the given scope.

    The runtime MrDocs embeds is a minimal ES engine that ships with
    none of the host environment scripts usually expect (no `console`,
    no `fs`, no `process`). This function fills in those gaps the same
    way Node and the browser do, so a script doesn't have to learn a
    MrDocs-specific replacement for each one.

    The set today is small (just `console`); each new global lands as a
    separate file under `src/lib/Js/` with its own `register*` entry
    point.
*/
void
registerStdGlobals(Scope& scope);

} // namespace mrdocs::js

#endif
