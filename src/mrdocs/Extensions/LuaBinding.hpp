//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_LUABINDING_HPP
#define MRDOCS_LIB_EXTENSIONS_LUABINDING_HPP

#include "LoadedExtensions.hpp"
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string>

namespace mrdocs {

class Config;

/** Load the extensions declared by one Lua script file.

    Build a fresh Lua engine and run the script's top level, which may
    register any number of corpus transforms with
    `mrdocs.register_transform(id, fn)` and output generators with
    `mrdocs.register_generator(id, fn)`. Nothing is applied here: the
    transforms are returned for the host to invoke later, and the
    generators are returned as self-contained @ref Generator objects. The
    returned engine handle keeps the VM alive for both. A script that
    registers nothing warns and returns an empty result. The run's
    `warn-as-error` setting is read from @p config so `mrdocs.report.warn`
    escalates like any other Mr.Docs warning.
*/
Expected<LoadedExtensions>
loadLuaExtensions(std::string const& scriptPath, Config const& config);

} // mrdocs

#endif
