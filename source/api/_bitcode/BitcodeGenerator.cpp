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
#include "api/Support/SafeNames.hpp"
#include "api/AST/Bitcode.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Metadata.hpp>

namespace clang {
namespace mrdox {
namespace bitcode {

class MultiFileBuilder : public Corpus::Visitor
{
    Corpus const& corpus_;
    Reporter& R_;
    std::string_view outputPath_;
    SafeNames names_;
    Config::WorkGroup wg_;

public:
    MultiFileBuilder(
        std::string_view outputPath,
        Corpus const& corpus,
        Reporter& R)
        : corpus_(corpus)
        , R_(R)
        , outputPath_(outputPath)
        , names_(corpus_)
        , wg_(&corpus.config)
    {
    }

    Err
    build()
    {
        corpus_.visit(globalNamespaceID, *this);
        wg_.wait();
        return Err();
    }

    template<class T>
    void build(T const& I)
    {
        namespace fs = llvm::sys::fs;
        namespace path = llvm::sys::path;

        wg_.post(
            [&]
            {
                llvm::SmallString<512> filePath(outputPath_);
                llvm::StringRef name = names_.get(I.id);
                path::append(filePath, name);
                filePath.append(".bc");
                std::error_code ec;
                llvm::raw_fd_ostream os(filePath, ec, fs::CD_CreateAlways);
                if(! R_.error(ec, "open '", filePath, "'"))
                {
                    auto bc = writeBitcode(I);
                    if(auto ec = os.error())
                        if(R_.error(ec, "write '", filePath, "'"))
                            return;
                    os.write(bc.data.data(), bc.data.size());
                }
            });
    }

    bool visit(NamespaceInfo const& I) override
    {
        corpus_.visit(I.Children, *this);
        return true;
    }

    bool visit(RecordInfo const& I) override
    {
        build(I);
        corpus_.visit(I.Children, *this);
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
    Reporter& R_;
    std::ostream& os_;

public:
    SingleFileBuilder(
        std::ostream& os,
        Corpus const& corpus,
        Reporter& R)
        : corpus_(corpus)
        , R_(R)
        , os_(os)
    {
    }

    Err
    build()
    {
        corpus_.visit(globalNamespaceID, *this);
        return Err();
    }

    template<class T>
    void build(T const& I)
    {
        auto bc = writeBitcode(I);
        os_.write(bc.data.data(), bc.data.size());
    }

    bool visit(NamespaceInfo const& I) override
    {
        corpus_.visit(I.Children, *this);
        return true;
    }

    bool visit(RecordInfo const& I) override
    {
        build(I);
        corpus_.visit(I.Children, *this);
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

Err
BitcodeGenerator::
build(
    std::string_view outputPath,
    Corpus const& corpus,
    Reporter& R) const
{
    return MultiFileBuilder(outputPath, corpus, R).build();
}

Err
BitcodeGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus,
    Reporter& R) const
{
    return SingleFileBuilder(os, corpus, R).build();
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
