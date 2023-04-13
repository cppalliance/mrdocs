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

#ifndef MRDOX_SOURCE_ASCIIDOC_HPP
#define MRDOX_SOURCE_ASCIIDOC_HPP

#include "Representation.h"
#include "Namespace.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Generator.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

namespace clang {
namespace mrdox {

struct OverloadSet;

class AsciidocGenerator
    : public Generator
{
public:
    class Writer;

    llvm::StringRef
    name() const noexcept override
    {
        return "Asciidoc";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "adoc";
    }

#if 0
    bool
    build(
        llvm::StringRef rootPath,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) const override;
#endif

    bool
    buildOne(
        llvm::StringRef fileName,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const override;

    bool
    buildString(
        std::string& dest,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const override;
};

//------------------------------------------------

class AsciidocGenerator::Writer
{
public:
    Writer(
        Corpus const& corpus,
        Config const& config,
        Reporter& R) noexcept
        : corpus_(corpus)
        , config_(config)
        , R_(R)
    {
    }

    void write(llvm::StringRef rootDir);
    void writeOne(llvm::raw_ostream& os);

    void writeAllSymbols();

    void write(RecordInfo const& I);
    void writeBase(BaseRecordInfo const& I);

    void write(FunctionInfo const& I);
    void write(
        llvm::StringRef sectionName,
        std::vector<OverloadSet> const& list);

    struct FormalParam;
    void write(FormalParam const& p, llvm::raw_ostream& os);
    FormalParam formalParam(FieldTypeInfo const& ft);

    struct TypeName;
    void write(TypeName const& tn, llvm::raw_ostream& os);
    TypeName typeName(TypeInfo const& ti);

    void openSection(llvm::StringRef name);
    void closeSection();

    static Location const& getLocation(SymbolInfo const& I);
    static llvm::StringRef toString(TagTypeKind k) noexcept;

private:
    struct Section
    {
        int level = 0;
        std::string markup;
    };

    Corpus const& corpus_;
    Config const& config_;
    Reporter& R_;
    llvm::raw_ostream* os_ = nullptr;
    Section sect_;
};

} // mrdox
} // clang

#endif
