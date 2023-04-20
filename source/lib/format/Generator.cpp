//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Commands.hpp"
#include <mrdox/format/Generator.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

bool
Generator::
build(
    StringRef outputPath,
    Corpus& corpus,
    Reporter& R) const
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // If we are given a filename with the correct
    // extension then just build the docs as one file.
    if(path::extension(outputPath).compare_insensitive(path::extension(outputPath)))
        return buildOne(outputPath, corpus, R);

    // Create the directory if needed
    fs::file_status status;
    std::error_code ec = fs::status(outputPath, status);
    if(ec == std::errc::no_such_file_or_directory)
    {
        ec = fs::create_directories(outputPath, false);
        if(R.error(ec, "create directories in '", outputPath, "'"))
            return false;
    }
    if(R.error(ec, "call fs::status on '", outputPath, "'"))
        return false;

    // If we are given an existing directory,
    // then build a single-page file there with
    // a default filename (e.g. "reference.adoc").
    if(fs::is_directory(outputPath))
    {
        llvm::SmallString<512> fileName(outputPath);
        path::append(fileName, "reference");
        path::replace_extension(fileName, extension());
        return buildOne(fileName, corpus, R);
    }

    // Build as one file
    return buildOne(outputPath, corpus, R);
}

} // mrdox
} // clang
