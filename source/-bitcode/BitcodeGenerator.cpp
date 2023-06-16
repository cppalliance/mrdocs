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

#include "BitcodeGenerator.hpp"
#include "Support/Error.hpp"
#include "Support/SafeNames.hpp"
#include "AST/Bitcode.hpp"
#include <mrdox/Support/Report.hpp>
#include <mrdox/Support/ThreadPool.hpp>
#include <mrdox/Metadata.hpp>

namespace clang {
namespace mrdox {
namespace bitcode {

class MultiFileBuilder : public Corpus::Visitor
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
        corpus_.traverse(*this, SymbolID::zero);
        auto errors = taskGroup_.wait();
        if(! errors.empty())
            return Error(errors);
        return Error::success();
    }

    template<class T>
    void build(T const& I)
    {
        namespace fs = llvm::sys::fs;
        namespace path = llvm::sys::path;

        taskGroup_.async(
            [&]
            {
                llvm::SmallString<512> filePath(outputPath_);
                llvm::StringRef name = names_.get(I.id);
                path::append(filePath, name);
                filePath.append(".bc");
                std::error_code ec;
                llvm::raw_fd_ostream os(filePath, ec, fs::CD_CreateAlways);
                if(ec)
                {
                    reportError(Error(ec), "open \"{}\"", filePath);
                    return;
                }
                auto bc = writeBitcode(I);
                if(auto ec = os.error())
                {
                    reportError(Error(ec), "write \"{}\"", filePath);
                    return;
                }
                os.write(bc.data.data(), bc.data.size());
            });
    }

    bool visit(NamespaceInfo const& I) override
    {
        corpus_.traverse(*this, I);
        return true;
    }

    bool visit(RecordInfo const& I) override
    {
        build(I);
        corpus_.traverse(*this, I);
        return true;
    }

    bool visit(FunctionInfo const& I) override
    {
        build(I);
        return true;
    }

    bool visit(TypedefInfo const& I) override
    {
        build(I);
        return true;
    }

    bool visit(EnumInfo const& I) override
    {
        build(I);
        return true;
    }
};

//------------------------------------------------

class SingleFileBuilder : public Corpus::Visitor
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
        corpus_.traverse(*this, SymbolID::zero);
        return Error();
    }

    template<class T>
    void build(T const& I)
    {
        auto bc = writeBitcode(I);
        os_.write(bc.data.data(), bc.data.size());
    }

    bool visit(NamespaceInfo const& I) override
    {
        corpus_.traverse(*this, I);
        return true;
    }

    bool visit(RecordInfo const& I) override
    {
        build(I);
        corpus_.traverse(*this, I);
        return true;
    }

    bool visit(FunctionInfo const& I) override
    {
        build(I);
        return true;
    }

    bool visit(TypedefInfo const& I) override
    {
        build(I);
        return true;
    }

    bool visit(EnumInfo const& I) override
    {
        build(I);
        return true;
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

} // mrdox
} // clang
