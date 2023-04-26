// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens D. Morgenstern
//

#include "TestConfig.hpp"
#include <mrdox/Support/Error.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>


template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::TestConfig>
{
    static void mapping(IO &io,
                        clang::mrdox::TestConfig& f)
    {
        io.mapOptional("cxxstd", f.cxxstd);
        io.mapOptional("compile-flags", f.compile_flags);
        io.mapOptional("should-fail", f.should_fail);
        io.mapOptional("heuristics", f.heuristics);
    }
};

namespace clang {
namespace mrdox {

Expected<std::vector<TestConfig>>
TestConfig::loadForTest(
    std::string_view dir,
    std::string_view file)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    llvm::SmallString<256> filePath{file.begin(), file.end()};
    path::replace_extension(filePath, "yml");

    if (!fs::exists(filePath))
    {
        filePath = dir;
        filePath.append("/mrdox-test.yml");
    }

    if (fs::exists(filePath))
    {
        auto fileText = llvm::MemoryBuffer::getFile(filePath);
        if(! fileText)
            return Error(fileText.getError());
        llvm::yaml::Input yin(**fileText);

        std::vector<TestConfig> res;
        do
        {
            yin >> res.emplace_back();
            if (auto ec = yin.error())
                return Error(ec);
        }
        while (yin.nextDocument());
        return res;
    }
    return std::vector<TestConfig>{TestConfig{}};
}

}
}