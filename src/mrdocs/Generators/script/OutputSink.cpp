//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "OutputSink.hpp"
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <fstream>
#include <ios>

namespace mrdocs::script {

OutputSink::
OutputSink(std::string_view outputDir)
    : root_(files::normalizePath(outputDir))
{
}

Expected<std::string>
OutputSink::
resolveUnderRoot(std::string_view relPath) const
{
    Expected<std::string> result;
    if (relPath.empty())
    {
        result = Unexpected(formatError(
            "output.write: path must not be empty"));
    }
    else if (files::isAbsolute(relPath))
    {
        result = Unexpected(formatError(
            "output.write: path '{}' must be relative", relPath));
    }
    else
    {
        std::string const full = files::normalizePath(
            files::appendPath(root_, relPath));
        if (!full.starts_with(root_))
        {
            result = Unexpected(formatError(
                "output.write: path '{}' escapes the output directory",
                relPath));
        }
        else
        {
            result = full;
        }
    }
    return result;
}

Expected<void>
OutputSink::
write(std::string_view relPath, std::string_view contents)
{
    MRDOCS_TRY(std::string full, resolveUnderRoot(relPath));
    MRDOCS_TRY(files::createDirectory(files::getParentDir(full)));

    std::ofstream os(full, std::ios::binary | std::ios::trunc);
    Expected<void> result;
    if (!os)
    {
        result = Unexpected(formatError(
            "output.write: cannot open '{}' for writing", full));
    }
    else
    {
        os.write(
            contents.data(),
            static_cast<std::streamsize>(contents.size()));
        if (!os)
        {
            result = Unexpected(formatError(
                "output.write: failed writing '{}'", full));
        }
    }
    return result;
}

} // mrdocs::script
