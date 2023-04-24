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

#ifndef MRDOX_LIB_CONFIGIMPL_HPP
#define MRDOX_LIB_CONFIGIMPL_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Config.hpp>
#include <llvm/Support/ThreadPool.h>
#include <memory>

namespace clang {
namespace mrdox {

class ConfigImpl
    : public Config
    , public std::enable_shared_from_this<ConfigImpl>

{
    std::vector<llvm::SmallString<0>> inputFileIncludes_;
    llvm::ThreadPool mutable threadPool_;
    bool doAsync_ = true;

    friend class Config;
    friend class Options;
    friend class WorkGroup;

    template<class T>
    friend struct llvm::yaml::MappingTraits;

    llvm::SmallString<0>
    normalizePath(llvm::StringRef pathName);

public:
    explicit
    ConfigImpl(
        llvm::StringRef configDir);

    void
    setSourceRoot(
        llvm::StringRef dirPath) override;

    void
    setInputFileIncludes(
        std::vector<std::string> const& list) override;

    //--------------------------------------------

    /** Returns true if the translation unit should be visited.

        @param filePath The posix-style full path
        to the file being processed.
    */
    bool
    shouldVisitTU(
        llvm::StringRef filePath) const noexcept;

    /** Returns true if the file should be visited.

        If the file is visited, then prefix is
        set to the portion of the file path which
        should be be removed for matching files.

        @param filePath The posix-style full path
        to the file being processed.

        @param prefix The prefix which should be
        removed from subsequent matches.
    */
    bool
    shouldVisitFile(
        llvm::StringRef filePath,
        llvm::SmallVectorImpl<char>& prefix) const noexcept;
};

} // mrdox
} // clang

#endif
