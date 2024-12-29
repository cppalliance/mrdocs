//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "VisitorHelpers.hpp"

namespace clang::mrdocs::hbs {

bool
shouldGenerate(Info const& I)
{
    if (I.isSpecialization())
    {
        return false;
    }
    if (I.isEnumConstant())
    {
        return false;
    }
    if (I.Extraction == ExtractionMode::Dependency)
    {
        return false;
    }
    if (I.Extraction == ExtractionMode::ImplementationDefined)
    {
        // We don't generate pages for implementation-defined symbols.
        // We do generate pages for see-below symbols.
        // See the requirements in ConfigOptions.json.
        return false;
    }
    return true;
}

} // clang::mrdocs::hbs
