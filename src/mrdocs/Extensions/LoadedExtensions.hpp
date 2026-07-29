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

#ifndef MRDOCS_LIB_EXTENSIONS_LOADEDEXTENSIONS_HPP
#define MRDOCS_LIB_EXTENSIONS_LOADEDEXTENSIONS_HPP

#include <mrdocs/Dom.hpp>
#include <mrdocs/Generator.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mrdocs {

/** The extensions one script file registered.

    A single script file can register any number of transforms and
    generators, so this holds all of them. Produced by the per-language
    loaders (`loadJsExtensions` / `loadLuaExtensions`) and aggregated by
    @ref ExtensionRegistry::load. `vm` is a strong, type-erased handle to
    the script's engine that keeps the VM alive for both the transforms and
    the generators, all of which hold only a weak reference to it.
*/
struct LoadedExtensions
{
    std::shared_ptr<void> vm;
    std::vector<std::pair<std::string, dom::Function>> transforms;
    std::vector<std::unique_ptr<Generator>> generators;
};

} // mrdocs

#endif // MRDOCS_LIB_EXTENSIONS_LOADEDEXTENSIONS_HPP
