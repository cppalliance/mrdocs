//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <llvm/Support/raw_ostream.h>
#include <format>


namespace mrdocs {

void
assert_failed(
    char const* msg,
    char const* file,
    std::uint_least32_t line)
{
  llvm::errs() << std::format("assertion failed: {} on line {} in {}\n", msg,
                              line, files::makeProjectRelative(file));
}

} // mrdocs

