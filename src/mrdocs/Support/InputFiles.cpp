//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "InputFiles.hpp"
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Filesystem/Glob.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <algorithm>
#include <string>

namespace mrdocs {

bool
isInputFile(Config const& config, std::string_view const filePath)
{
    // Inclusion is generous: a file counts as being inside an input
    // directory when its path matches as written or by its real
    // (symlink-resolved) location. This recognizes files reached through a
    // symlinked directory without dropping anything that
    // already matched as written.
    if (config.recursive)
    {
        MRDOCS_CHECK_OR(
            config.input.empty() ||
            std::ranges::any_of(config.input,
                [&](std::string const& inputDir)
                {
                    return files::isResolvedSubpathOf(filePath, inputDir);
                }),
            false);
    }
    else
    {
        // Resolve the file's parent lazily: the filesystem lookup only
        // happens when a literal match fails, so a tree with no symlinks
        // pays no extra cost.
        std::string_view const fileParentDir = files::getParentDir(filePath);
        Optional<std::string> fileParentDirReal;
        auto parentDirReal = [&]() -> std::string const&
        {
            if (!fileParentDirReal)
            {
                fileParentDirReal = files::makeRealPath(fileParentDir);
            }
            return *fileParentDirReal;
        };
        MRDOCS_CHECK_OR(
            config.input.empty() ||
            std::ranges::any_of(config.input,
                [&](std::string const& inputDir)
                {
                    return inputDir == fileParentDir
                        || files::makeRealPath(inputDir) == parentDirReal();
                }),
            false);
    }

    // Don't extract declarations that fail the file pattern filter
    MRDOCS_CHECK_OR(
        config.filePatterns.empty() ||
        std::ranges::any_of(config.filePatterns,
        [fileName = files::getFileName(filePath)]
        (PathGlobPattern const& pattern)
            {
                return pattern.match(fileName);
            }),
        false);

    return true;
}

} // mrdocs
