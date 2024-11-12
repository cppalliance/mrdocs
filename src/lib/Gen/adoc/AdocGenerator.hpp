//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_ADOC_ADOCGENERATOR_HPP
#define MRDOCS_LIB_GEN_ADOC_ADOCGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Generator.hpp>
#include <lib/Gen/hbs/HandlebarsGenerator.hpp>

namespace clang {
namespace mrdocs {
namespace adoc {

class AdocGenerator
    : public hbs::HandlebarsGenerator
{
public:
    AdocGenerator();
};

} // adoc
} // mrdocs
} // clang

#endif
