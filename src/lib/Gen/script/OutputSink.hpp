//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_SCRIPT_OUTPUTSINK_HPP
#define MRDOCS_LIB_GEN_SCRIPT_OUTPUTSINK_HPP

#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <string>
#include <string_view>

namespace mrdocs::script {

/** The file-writing API handed to a script-driven generator.

    A script-driven generator owns its output structure: it decides
    which files to write and what to put in them. This class is the only
    door it has to the filesystem, bound into the script as the `write`
    method of the `output` object. Every path is resolved under a single
    output directory and may not escape it; i.e., a script cannot write
    "anywhere on disk".
*/
class OutputSink
{
    // The output directory, normalized and absolute, without a trailing
    // separator.
    std::string root_;

    // Resolve `relPath` under the output directory. Reject an empty path,
    // an absolute path, or a path that escapes the directory.
    Expected<std::string>
    resolveUnderRoot(std::string_view relPath) const;

public:
    /** Construct a sink rooted at the given output directory.
    */
    explicit
    OutputSink(std::string_view outputDir);

    /** Write `contents` to `relPath`, resolved under the output directory.

        Create any missing parent directories. Reject an absolute path
        or one that escapes the output directory.

        @param relPath The destination path, relative to the output
            directory.
        @param contents The bytes to write.
        @return Success, or an error describing why the write failed.
    */
    Expected<void>
    write(std::string_view relPath, std::string_view contents);
};

} // mrdocs::script

#endif
