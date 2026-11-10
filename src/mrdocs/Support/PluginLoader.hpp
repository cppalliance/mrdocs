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

#ifndef MRDOCS_LIB_SUPPORT_PLUGINLOADER_HPP
#define MRDOCS_LIB_SUPPORT_PLUGINLOADER_HPP

// The native half of `ExtensionRegistry::loadExtensions`: the one place a
// shared library is opened. AddonDiscovery.hpp decides which libraries to
// load, and in what order.

#include <mrdocs/Config.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string>

namespace mrdocs {

/** Load a native extension and run its entry point, unless it ran already.

    The library is loaded for the lifetime of the process and its entry
    point is called once per library registry, so that what the plugin
    installs is in place before anything looks for it. Loading the same
    library again, into another registry, adds the transforms and
    generators it installed the first time instead of running the entry
    point a second time. The transforms go to `extensions`, once the entry
    point has succeeded, and the generators as the entry point installs
    them. A generator whose id
    is taken in `extensions`, whether by one that ships with MrDocs, by a
    script loaded earlier or by another plugin, is refused.

    These cases are an error: a library that cannot be loaded, does not
    export the entry points, was built against another build of MrDocs or
    with another standard library, or reports an error of its own. A
    plugin is there because the user put it there, so the loader does not
    silently accept one that does nothing.

    @par Thread Safety
    One library's entry point runs once per library registry, whichever
    call gets there first. Calls that load into one registry from several
    threads are not supported.

    @return The error, if any occurred.

    @param path The library file, a posix-style UTF-8 path.

    @param directory The directory of the extension when the library sits
    in one. It holds the libraries the extension depends on (Windows adds
    it to the search for them). Empty when the library is directly in
    `extensions/`.

    @param config The configuration the plugin reads.

    @param extensions The registry the plugin's transforms and generators
    are added to.

    @param libraries The record of the libraries loaded in this process.

    @param reporter The reporter of the run, which prints the plugins
    loaded.
*/
Expected<void>
loadPlugin(
    std::string const& path,
    std::string const& directory,
    Config const& config,
    ExtensionRegistry& extensions,
    ExtensionRegistry::LibraryRegistry& libraries,
    report::Reporter& reporter);

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_PLUGINLOADER_HPP
