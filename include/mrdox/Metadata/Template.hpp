//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_TEMPLATE_HPP
#define MRDOX_METADATA_TEMPLATE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/TemplateArg.hpp>
#include <mrdox/Metadata/TemplateParam.hpp>

namespace clang {
namespace mrdox {

enum class TemplateSpecKind
{
    Primary = 0, // for bitstream
    Explicit,
    Partial
};

// stores information pertaining to primary template and
// partial/explicit specialization declarations
struct TemplateInfo
{
    // for primary templates:
    //     - Params will be non-empty
    //     - Args will be empty
    // for explicit specializations:
    //     - Params will be empty
    // for partial specializations:
    //     - Params will be non-empty
    //     - each template parameter will appear at least
    //       once in Args outside of a non-deduced context
    std::vector<TParam> Params;
    std::vector<TArg> Args;

    // stores the ID of the corresponding primary template
    // for partial and explicit specializations
    OptionalSymbolID Primary;

    // KRYSTIAN NOTE: using the presence of args/params
    // to determine the specialization kind *should* work.
    // emphasis on should.
    TemplateSpecKind specializationKind() const noexcept
    {
        if(Params.empty())
            return TemplateSpecKind::Explicit;
        return Args.empty() ? TemplateSpecKind::Primary :
            TemplateSpecKind::Partial;
    }
};

} // mrdox
} // clang

#endif
