//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_GENERATOR_HPP
#define MRDOCS_LIB_SUPPORT_GENERATOR_HPP

#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error.hpp>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>

namespace mrdocs {

// Output-resolution and file-writing helpers shared by the built-in
// generators (and the golden-test comparison). These are MrDocs internals,
// not part of the public, plugin-facing Generator interface.

/** Resolve the output location for a generator from the configuration.

    A per-generator `output` (under `generator-options.<id>`) is used as
    given. Otherwise the top-level `output` is used directly for a single
    generator, or a subdirectory named after the generator id when several
    run, so each writes into its own directory. All paths are resolved
    relative to the configuration file.

    Generators call this from @ref Generator::build to find where their
    files go; they remain free to interpret the result as a file or a
    directory.

    @return The absolute output path for the generator.

    @param generator The generator whose output is being resolved.
    @param corpus The corpus being built; its config drives the result.
*/
std::string
getGeneratorOutputPath(
    Generator const& generator,
    Corpus const& corpus);

/** Return the full path for single page output.

    Resolves the file a single-page generator writes from `outputPath`. When
    `outputPath` looks like a file it is used directly; when it looks like a
    directory the page is `reference.<extension>` inside it.

    @return The full path to the single-page output file.

    @param outputPath The output path, interpreted as a file or a directory.
    @param extension The file extension to use for single-page output.
*/
Expected<std::string>
getSinglePageFullPath(
    std::string_view outputPath,
    std::string_view extension);

/** Open a file and write to it through a callback.

    Creates the parent directory, opens `fileName` for writing (truncating
    an existing file), and invokes `render` with the output stream.

    @return The error, if any occurred.

    @param fileName The file to write; it is overwritten if it exists.
    @param render Invoked with the open output stream; its result is
    returned.
*/
Expected<void>
writeToFile(
    std::string_view fileName,
    std::function<Expected<void>(std::ostream&)> render);

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_GENERATOR_HPP
