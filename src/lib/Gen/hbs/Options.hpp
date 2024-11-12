//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HBS_OPTIONS_HPP
#define MRDOCS_LIB_GEN_HBS_OPTIONS_HPP

#include <mrdocs/Support/Error.hpp>
#include <string>

namespace clang {
namespace mrdocs {

class Corpus;

namespace hbs {

/** Generator-specific options.
*/
struct Options
{
    /** True if the generator should produce legible names.

        Legible names are symbol names safe for URLs and
        file names.

        When disabled, the generator produces hashes
        for symbol references.
    */
    bool legible_names = true;

    /** Directory with the templates used to generate the output.

        The templates are written in Handlebars.

        The default directory is `generator/<fileExtension>`
        relative to the addons directory.
    */
    std::string template_dir;
};

} // hbs
} // mrdocs
} // clang

#endif
