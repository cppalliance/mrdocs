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

#include "AST/Commands.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Generator.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fstream>
#include <sstream>

namespace clang {
namespace mrdox {

Generator::
~Generator() noexcept = default;

/*  default implementation of this function
    assumes the output is single page, and emits
    the file reference.ext using the extension
    of the generator.
*/
Err
Generator::
build(
    std::string_view outputPath,
    Corpus const& corpus,
    Reporter& R) const
{
    namespace path = llvm::sys::path;

    using SmallString = llvm::SmallString<0>;

    SmallString ext(".");
    ext += fileExtension();

    SmallString fileName(outputPath);
    if(path::extension(outputPath).compare_insensitive(ext) != 0)
    {
        // directory specified
        path::append(fileName, "reference");
        path::replace_extension(fileName, ext);
    }

    return buildOne(fileName.str(), corpus, R);
}

Err
Generator::
buildOne(
    std::string_view fileName,
    Corpus const& corpus,
    Reporter& R) const
{
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
        return makeErr("std::ofstream threw ", ex.what());
    }

    try
    {
        return buildOne(os, corpus, R);
    }
    catch(std::exception const& ex)
    {
        return makeErr("buildOne threw ", ex.what() );
    }
}

Err
Generator::
buildOneString(
    std::string& dest,
    Corpus const& corpus,
    Reporter& R) const
{
    dest.clear();
    std::stringstream ss;
    try
    {
        auto err = buildOne(ss, corpus, R);
        if(err)
            return err;
        dest = ss.str();
        return {};
    }
    catch(std::exception const& ex)
    {
        return makeErr("buildOne threw ", ex.what() );
    }
}

} // mrdox
} // clang
