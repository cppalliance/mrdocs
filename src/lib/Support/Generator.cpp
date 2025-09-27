//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/AST/ParseJavadoc.hpp>
#include <lib/Support/Chrono.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fstream>
#include <sstream>


namespace mrdocs {

Generator::
~Generator() noexcept = default;

/*  Default implementation of this function
    assumes the output is single page, and emits
    the file reference.ext using the extension
    of the generator.
*/
Expected<void>
Generator::
build(
    std::string_view outputPath,
    Corpus const& corpus) const
{
    auto const fileName = getSinglePageFullPath(outputPath, fileExtension());
    MRDOCS_CHECK_OR(fileName, Unexpected(fileName.error()));
    return buildOne(*fileName, corpus);
}

Expected<void>
Generator::
build(Corpus const& corpus) const
{
    using clock_type = std::chrono::steady_clock;
    auto const start_time = clock_type::now();
    std::string const absOutput = files::normalizePath(
        files::makeAbsolute(
            corpus.config->output,
            corpus.config->configDir()));
    MRDOCS_TRY(build(absOutput, corpus));
    report::info(
        "Generated {} documentation in {}",
        this->displayName(),
        format_duration(clock_type::now() - start_time));
    return {};
}

Expected<void>
Generator::
buildOne(
    std::string_view fileName,
    Corpus const& corpus) const
{
    std::string dir = files::getParentDir(fileName);
    MRDOCS_TRY(files::createDirectory(dir));

    std::ofstream os;
    try
    {
        os.open(std::string(fileName),
            std::ios_base::binary |
                std::ios_base::out |
                std::ios_base::trunc // | std::ios_base::noreplace
            );
    }
    catch(std::exception const& ex)
    {
        return Unexpected(formatError("std::ofstream threw \"{}\"", ex.what()));
    }

    try
    {
        return buildOne(os, corpus);
    }
    catch(std::exception const& ex)
    {
        return Unexpected(formatError("buildOne threw \"{}\"", ex.what()));
    }
}

Expected<void>
Generator::
buildOneString(
    std::string& dest,
    Corpus const& corpus) const
{
    dest.clear();
    std::stringstream ss;
    try
    {
        MRDOCS_TRY(buildOne(ss, corpus));
        dest = ss.str();
        return {};
    }
    catch(Exception const& ex)
    {
        return Unexpected(ex.error());
    }
    catch(std::exception const& ex)
    {
        return Unexpected(formatError("buildOne threw \"{}\"", ex.what()));
    }
}

Expected<std::string>
getSinglePageFullPath(
    std::string_view outputPath,
    std::string_view extension)
{
    namespace path = llvm::sys::path;
    using SmallString = llvm::SmallString<0>;

    SmallString ext(".");
    ext += extension;

    if (files::exists(outputPath))
    {
        // If the path exists and is a directory, append the default file
        if (files::isDirectory(outputPath))
        {
            SmallString fileName(outputPath);
            path::append(fileName, "reference");
            path::replace_extension(fileName, ext);
            return fileName.str().str();
        }
        // If the path exists and is a file, use it directly
        return std::string(outputPath);
    }

    // If the path does not exist, check if it has an extension
    SmallString fileName(outputPath);
    if (!path::extension(fileName).empty())
    {
        // Path has an extension, treat it as a file
        return fileName.str().str();
    }

    // Path does not have an extension, assume it's a directory
    path::append(fileName, "reference");
    path::replace_extension(fileName, ext);
    return fileName.str().str();
}

} // mrdocs

