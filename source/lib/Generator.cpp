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

#include <mrdox/Generator.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

bool
Generator::
build(
    StringRef rootPath,
    Corpus const& corpus,
    Config const& cfg,
    Reporter& R) const
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // If we are given a filename with the correct
    // extension then just build the docs as one file.
    if(path::extension(rootPath).compare_insensitive(path::extension(rootPath)))
        return buildOne(rootPath, corpus, cfg, R);

    // Create the directory if needed
    fs::file_status status;
    std::error_code ec = fs::status(rootPath, status);
    if(ec == std::errc::no_such_file_or_directory)
    {
        ec = fs::create_directories(rootPath, false);
        if(ec)
        {
            R.failed("fs::create_directories", ec);
            return false;
        }
    }
    else if(ec)
    {
        R.failed("fs::status", ec);
        return false;
    }

    // If we are given an existing directory,
    // then build a single-page file there with
    // a default filename (e.g. "index.adoc").
    if(fs::is_directory(rootPath))
    {
        llvm::SmallString<512> fileName(rootPath);
        path::append(fileName, "index");
        path::replace_extension(fileName, extension());
        return buildOne(fileName, corpus, cfg, R);
    }

    // Build as one file
    return buildOne(rootPath, corpus, cfg, R);
}

//------------------------------------------------

std::string
getTagType(TagTypeKind AS)
{
    switch (AS) {
    case TagTypeKind::TTK_Class:
        return "class";
    case TagTypeKind::TTK_Union:
        return "union";
    case TagTypeKind::TTK_Interface:
        return "interface";
    case TagTypeKind::TTK_Struct:
        return "struct";
    case TagTypeKind::TTK_Enum:
        return "enum";
    }
    llvm_unreachable("Unknown TagTypeKind");
}

} // mrdox
} // clang
