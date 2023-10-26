//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_ADOC_OPTIONS_HPP
#define MRDOCS_LIB_ADOC_OPTIONS_HPP

#include <mrdocs/Support/Error.hpp>
#include <string>

namespace clang {
namespace mrdocs {

class Corpus;

namespace adoc {

/** Generator-specific options.
*/
struct Options
{
    bool safe_names = true;
    std::string template_dir;
};

/** Return loaded Options from a configuration.
*/
Expected<Options>
loadOptions(
    Corpus const& corpus);

} // adoc
} // mrdocs
} // clang

#endif
