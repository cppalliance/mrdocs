//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_ENGINES_CONSOLE_HPP
#define MRDOCS_LIB_ENGINES_CONSOLE_HPP

#include <mrdocs/Engines/JavaScript.hpp>

namespace mrdocs::js {

/** Install a `console` global on the given scope.

    Exposes `console.log` (to stdout) and `console.error` (to stderr).
    Both accept any number of arguments, render each one (`JSON.stringify`
    for objects and arrays, the DOM's `toString` for everything else),
    join with spaces, and finish with a newline. The shape matches
    Node's and the browser's so scripts can rely on the same call
    signature they already know.
*/
void
registerConsole(Scope& scope);

} // namespace mrdocs::js

#endif
