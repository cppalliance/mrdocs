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
#include <mrdox/Error.hpp>
#include <mrdox/Generator.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

llvm::Error
Generator::
buildPages(
    llvm::StringRef outputPath,
    Corpus const& corpus,
    Reporter& R) const
{
    namespace path = llvm::sys::path;

    using SmallString = llvm::SmallString<0>;

    SmallString ext(".");
    ext += extension();

    SmallString fileName = outputPath;
    if(path::extension(outputPath).compare_insensitive(ext) != 0)
    {
        // directory specified
        path::append(fileName, "reference");
        path::replace_extension(fileName, ext);
    }

    std::error_code ec;
    llvm::raw_fd_ostream os(fileName, ec);
    if(ec)
        return makeError("raw_fd_ostream returned ", ec);
    return buildSinglePage(os, corpus, R, &os);
}

llvm::Error
Generator::
buildSinglePageFile(
    llvm::StringRef filePath,
    Corpus const& corpus,
    Reporter& R) const
{
    std::error_code ec;
    llvm::raw_fd_ostream os(filePath, ec);
    if(ec)
        return makeError("raw_fd_ostream returned ", ec);
    return buildSinglePage(os, corpus, R, &os);
}

llvm::Error
Generator::
buildSinglePageString(
    std::string& dest,
    Corpus const& corpus,
    Reporter& R) const
{
    dest.clear();
    llvm::raw_string_ostream os(dest);
    return buildSinglePage(os, corpus, R);
}

} // mrdox
} // clang
