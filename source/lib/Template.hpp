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

#ifndef MRDOX_TEMPLATE_HPP
#define MRDOX_TEMPLATE_HPP

#include "TemplateParam.hpp"
#include <llvm/ADT/Optional.h>

namespace clang {
namespace mrdox {

struct TemplateSpecializationInfo
{
    // Indicates the declaration that this specializes.
    SymbolID SpecializationOf;

    // Template parameters applying to the specialized record/function.
    std::vector<TemplateParamInfo> Params;
};

// Records the template information for a struct or function that is a template
// or an explicit template specialization.
struct TemplateInfo
{
    // May be empty for non-partial specializations.
    std::vector<TemplateParamInfo> Params;

    // Set when this is a specialization of another record/function.
    llvm::Optional<TemplateSpecializationInfo> Specialization;
};

} // mrdox
} // clang

#endif
