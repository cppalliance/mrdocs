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

#ifndef MRDOCS_LIB_EXTENSIONS_ADDONDISCOVERY_HPP
#define MRDOCS_LIB_EXTENSIONS_ADDONDISCOVERY_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string>
#include <vector>

namespace mrdocs {

/** The language of an extension's entry file.
*/
enum class ExtensionKind
{
    /// A Lua script (`.lua`).
    Lua,
    /// A JavaScript script (`.js`).
    JavaScript,
    /// A native shared library (`.so`, `.dylib` or `.dll`).
    Native
};

/** One extension found under an addon root.

    @see collectExtensions
*/
struct ExtensionEntry
{
    /// The language of the entry file.
    ExtensionKind kind = ExtensionKind::Lua;

    /// The entry file: a posix-style UTF-8 path.
    std::string path;

    /// The directory of the extension when the entry file sits in one
    /// (`extensions/<name>/`), and empty when the entry is a file
    /// directly in `extensions/`.
    std::string directory;
};

/** Return the extensions under the `extensions/` directory of each root.

    Every immediate child of an `extensions/` directory is a candidate.
    A regular file ending in `.lua` or `.js` is a script entry, and one
    ending in the extension of a loadable library on this platform is a
    native entry. A symbolic link to such a file, or one whose target is
    missing, counts as the file does, so that loading it fails and names
    it.

    A directory `<name>/` is an extension whose entry file is one of
    `<name>.lua`, `init.lua`, `<name>.js`, `index.js`, `<name>.so`,
    `lib<name>.so`, `<name>.dylib`, `lib<name>.dylib` and `<name>.dll`
    that exists in it, restricted to the library names this platform
    loads. A directory without an entry file is skipped, with a debug
    message. When a directory holds both names of one language, the
    convention of the language wins: `init.lua` over `<name>.lua` and
    `index.js` over `<name>.js`, and a debug message names the file left
    out. A directory with more than one entry file otherwise, such as a
    Lua and a JavaScript file or two library names, is an error that names
    the directory and the candidates.

    An extension is named by its stem: the file name without its suffix
    (`.lua`, `.js`, `.so`, `.dylib`, `.dll`) and without the `lib` prefix
    of a native library, or the name of a directory extension. Two
    entries of one `extensions/` directory with the same name are an
    error that names the directory and the entries, before anything
    loads: `<dir>: more than one extension is named "X" (a.lua, b.js)`. Across roots, an entry in a later root replaces the entry with
    the same name from an earlier root whatever its kind, so
    `brief-filler.js` or a `brief-filler/` directory in a later root
    replaces `brief-filler.lua` from an earlier one, and `libstats.so`
    replaces `stats.so` (and `stats.dylib` where both suffixes name a
    library). A directory with a native entry loses the `lib` prefix too.
    Names ignore case on Windows and macOS, as file names do there. Extensions with different names are never
    replaced, even if they register the same id. A native entry that is
    the same file as an earlier one under another name, for example
    through a link, is reported once.

    The entries of every root form one list. The order of the list is
    not specified and callers may not rely on it; today it is sorted by
    extension name compared as bytes.

    An `extensions` entry that is not a readable directory is reported
    as a warning and contributes nothing.

    @return The entries, in today's order, or the error for an ambiguous
    directory.

    @param roots The addon roots, in the order the configuration lists
    them.

    @param reporter The reporter of the warnings and of the skipped
    directories.
*/
Expected<std::vector<ExtensionEntry>>
collectExtensions(
    std::vector<std::string> const& roots,
    report::Reporter& reporter);

/** Return the extensions under the addon roots of a configuration.

    @copydetails collectExtensions(std::vector<std::string> const&, report::Reporter&)

    @param config The configuration whose addon roots are searched:
    `addons`, then each entry of `addons-supplemental`.
*/
Expected<std::vector<ExtensionEntry>>
collectExtensions(Config const& config, report::Reporter& reporter);

} // mrdocs

#endif
