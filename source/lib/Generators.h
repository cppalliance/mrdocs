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

// Generator classes for converting declaration
// information into documentation in a specified format.

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_GENERATOR_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_GENERATOR_H

#include "Representation.h"
#include "jad/Index.hpp"
#include "llvm/Support/Error.h"
#include "llvm/Support/Registry.h"

namespace clang {
namespace mrdox {

struct Config;
struct Corpus;

/** The representation of the source code under analysis.
*/
using InfoMap = llvm::StringMap<
    std::unique_ptr<mrdox::Info>>;

class Generator
{
public:
    virtual ~Generator() = default;

    // Write out the decl info for the objects in
    // the given map in the specified format.
    virtual
    llvm::Error
    generateDocs(
        StringRef RootDir,
        Corpus const& corpus,
        Config const& cfg) = 0;

    // This function writes a file with the index previously constructed.
    // It can be overwritten by any of the inherited generators.
    // If the override method wants to run this it should call
    // Generator::createResources(cfg);
    virtual
    llvm::Error
    createResources(
        Config& cfg,
        Corpus& corpus);

    // Write out one specific decl info to the destination stream.
    virtual
    llvm::Error
    generateDocForInfo(
        Info* I, // VFALCO Why not const?
        llvm::raw_ostream& OS,
        Config const& cfg) = 0;
};

// VFALCO a global?
typedef llvm::Registry<Generator> GeneratorRegistry;

llvm::Expected<std::unique_ptr<Generator>>
findGeneratorByName(llvm::StringRef Format);

std::string getTagType(TagTypeKind AS);

} // namespace mrdox
} // namespace clang

#endif
