//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_INPUTFILES_HPP
#define MRDOCS_LIB_SUPPORT_INPUTFILES_HPP

#include <mrdocs/Config.hpp>
#include <string_view>

namespace mrdocs {

/** Determine whether a file is one of the inputs.

    A file is one of the inputs when it lies in an `input` directory and
    matches the `file-patterns`. The exclusions play no part: an excluded
    file is still one of the author's own, where a file outside the inputs
    belongs to someone else, even when it sits under `source-root`.

    @param config The configuration naming the inputs.
    @param filePath The absolute path of the file.
    @return Whether the file is one of the inputs.
*/
bool
isInputFile(Config const& config, std::string_view filePath);

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_INPUTFILES_HPP
