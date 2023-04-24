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

#include "AdocGenerator.hpp"
#include "AdocMultiPageWriter.hpp"
#include "AdocPagesBuilder.hpp"
#include "AdocSinglePageWriter.hpp"

namespace clang {
namespace mrdox {
namespace adoc {

//------------------------------------------------
//
// AdocGenerator
//
//------------------------------------------------

llvm::Error
AdocGenerator::
buildPages(
    llvm::StringRef outputPath,
    Corpus const& corpus,
    Reporter& R) const
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

#if 0
    // Track which directories we already tried to create.
    llvm::StringSet<> CreatedDirs;

    // Collect all output by file name and create the necessary directories.
    llvm::StringMap<std::vector<mrdox::Info*>> FileToInfos;
    for (const auto& Group : corpus.InfoMap)
    {
        mrdox::Info* Info = Group.getValue().get();

        llvm::SmallString<128> Path;
        llvm::sys::path::native(RootDir, Path);
        llvm::sys::path::append(Path, Info->getRelativeFilePath(""));
        if (!CreatedDirs.contains(Path)) {
            if (std::error_code Err = llvm::sys::fs::create_directories(Path);
                Err != std::error_code()) {
                return llvm::createStringError(Err, "Failed to create directory '%s'.",
                    Path.c_str());
            }
            CreatedDirs.insert(Path);
        }

        llvm::sys::path::append(Path, Info->getFileBaseName() + ".adoc");
        FileToInfos[Path].push_back(Info);
    }

    for (const auto& Group : FileToInfos) {
        std::error_code FileErr;
        llvm::raw_fd_ostream InfoOS(Group.getKey(), FileErr,
            llvm::sys::fs::OF_None);
        if (FileErr) {
            return llvm::createStringError(FileErr, "Error opening file '%s'",
                Group.getKey().str().c_str());
        }

        for (const auto& Info : Group.getValue()) {
            if (llvm::Error Err = generateDocForInfo(Info, InfoOS, config)) {
                return Err;
            }
        }
    }
    return true;
#endif
    AdocPagesBuilder builder(corpus);
    builder.scan();

    return llvm::Error::success();
}

llvm::Error
AdocGenerator::
buildSinglePage(
    llvm::raw_ostream& os,
    Corpus const& corpus,
    Reporter& R,
    llvm::raw_fd_ostream* fd_os) const
{
    AdocSinglePageWriter w(os, fd_os, corpus, R);
    return w.build();
}

} // adoc

//------------------------------------------------

std::unique_ptr<Generator>
makeAdocGenerator()
{
    return std::make_unique<adoc::AdocGenerator>();
}

} // mrdox
} // clang
