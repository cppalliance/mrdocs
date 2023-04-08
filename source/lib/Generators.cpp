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

#include "Generators.h"

LLVM_INSTANTIATE_REGISTRY(clang::mrdox::GeneratorRegistry)

namespace clang {
namespace mrdox {

llvm::Expected<std::unique_ptr<Generator>>
findGeneratorByName(llvm::StringRef Format) {
    for (const auto& Generator : GeneratorRegistry::entries()) {
        if (Generator.getName() != Format)
            continue;
        return Generator.instantiate();
    }
    return createStringError(llvm::inconvertibleErrorCode(),
        "can't find generator: " + Format);
}

// Enum conversion

std::string getTagType(TagTypeKind AS) {
    switch (AS) {
    case TagTypeKind::TTK_Class:
        return "class";
    case TagTypeKind::TTK_Union:
        return "union";
    case TagTypeKind::TTK_Interface:
        return "interface";
    case TagTypeKind::TTK_Struct:
        return "struct";
    case TagTypeKind::TTK_Enum:
        return "enum";
    }
    llvm_unreachable("Unknown TagTypeKind");
}

llvm::Error
Generator::
createResources(
    Config& cfg,
    Corpus& corpus)
{
    return llvm::Error::success();
}

// This anchor is used to force the linker to link in the generated object file
// and thus register the generators.
//
// VFALCO REDO THIS it spreads out the generator
//        particulars across too many separate source files.
//
extern volatile int AsciidocGeneratorAnchorSource;
static int LLVM_ATTRIBUTE_UNUSED AsciidocGeneratorAnchorDest =
AsciidocGeneratorAnchorSource;

extern volatile int XMLGeneratorAnchorSource;
static int LLVM_ATTRIBUTE_UNUSED XMLGeneratorAnchorDest =
AsciidocGeneratorAnchorSource;

} // namespace mrdox
} // namespace clang
