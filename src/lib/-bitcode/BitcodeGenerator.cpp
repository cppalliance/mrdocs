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

#include "BitcodeGenerator.hpp"
#include "lib/Support/Error.hpp"
#include "lib/Support/SafeNames.hpp"
#include "lib/AST/Bitcode.hpp"
#include <mrdocs/Support/ThreadPool.hpp>
#include <mrdocs/Metadata.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdocs {
namespace bitcode {

class MultiFileBuilder
{
    Corpus const& corpus_;
    std::string_view outputPath_;
    SafeNames names_;
    TaskGroup taskGroup_;

public:
    MultiFileBuilder(
        std::string_view outputPath,
        Corpus const& corpus)
        : corpus_(corpus)
        , outputPath_(outputPath)
        , names_(corpus_)
        , taskGroup_(corpus.config.threadPool())
    {
    }

    Error
    build()
    {
        corpus_.traverse(
            corpus_.globalNamespace(), *this);
        auto errors = taskGroup_.wait();
        if(! errors.empty())
            return Error(errors);
        return Error::success();
    }

    template<class T>
    void operator()(T const& I)
    {
        namespace fs = llvm::sys::fs;
        namespace path = llvm::sys::path;

        taskGroup_.async(
            [&]
            {
                llvm::SmallString<512> filePath(outputPath_);
                path::append(filePath, names_.getUnqualified(I.id));
                filePath.append(".bc");
                std::error_code ec;
                llvm::raw_fd_ostream os(filePath, ec, fs::CD_CreateAlways);
                if(ec)
                    Error(ec).Throw();
                auto bc = writeBitcode(I);
                if((ec = os.error()))
                    Error(ec).Throw();
                os.write(bc.data(), bc.size());
                if((ec = os.error()))
                    Error(ec).Throw();
            });

        if constexpr(T::isRecord())
            corpus_.traverse(I, *this);
    }
};

//------------------------------------------------

class SingleFileBuilder
{
    Corpus const& corpus_;
    std::ostream& os_;

public:
    SingleFileBuilder(
        std::ostream& os,
        Corpus const& corpus)
        : corpus_(corpus)
        , os_(os)
    {
    }

    Error
    build()
    {
        corpus_.traverse(corpus_.globalNamespace(), *this);
        return Error::success();
    }

    template<class T>
    void
    operator()(T const& I)
    {
        auto bc = writeBitcode(I);
        os_.write(bc.data(), bc.size());
        if constexpr(T::isRecord())
            corpus_.traverse(I, *this);
    }
};

//------------------------------------------------

Error
BitcodeGenerator::
build(
    std::string_view outputPath,
    Corpus const& corpus) const
{
    return MultiFileBuilder(outputPath, corpus).build();
}

Error
BitcodeGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus) const
{
    return SingleFileBuilder(os, corpus).build();
}

} // xml

//------------------------------------------------

std::unique_ptr<Generator>
makeBitcodeGenerator()
{
    return std::make_unique<bitcode::BitcodeGenerator>();
}

} // mrdocs
} // clang
