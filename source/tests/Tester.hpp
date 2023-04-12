//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TESTS_TESTER_HPP
#define MRDOX_TESTS_TESTER_HPP

// Each test comes as a pair of files.
// A .cpp file containing valid declarations,
// and a .xml file containing the expected output
// of the XML generator, which must match exactly.

#include <mrdox/Config.hpp>
#include <mrdox/Generator.hpp>
#include <mrdox/Reporter.hpp>
#include <llvm/Support/ThreadPool.h>
#include <memory>

namespace clang {
namespace mrdox {

class Tester
{
    Config const& config_;
    std::unique_ptr<Generator> xmlGen;
    std::unique_ptr<Generator> adocGen;
    Reporter& R_;

public:
    explicit
    Tester(
        Config const& config,
        Reporter &R);

    bool
    checkDirRecursively(
        llvm::SmallString<340> dirPath,
        llvm::ThreadPool& threadPool);

    void
    checkOneFile(
        Corpus& corpus,
        llvm::StringRef inputPath,
        llvm::SmallVectorImpl<char>& outputPathStr);
};

} // mrdox
} // clang

#endif
