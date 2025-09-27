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

#include "AdocGenerator.hpp"
#include "AdocEscape.hpp"
#include <mrdocs/Support/Handlebars.hpp>

namespace mrdocs {
namespace adoc {

void
AdocGenerator::
escape(OutputRef& os, std::string_view const str) const
{
    AdocEscape(os, str);
}

} // adoc

std::unique_ptr<Generator>
makeAdocGenerator()
{
    return std::make_unique<adoc::AdocGenerator>();
}

} // mrdocs
