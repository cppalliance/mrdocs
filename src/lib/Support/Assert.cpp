//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Assert.hpp>
#include <mrdocs/Support/Path.hpp>
#include <fmt/format.h>
#include <llvm/Support/raw_ostream.h>

namespace SourceFileNames {
extern char const* getFileName(char const*) noexcept;
} // SourceFileNames

namespace clang {
namespace mrdocs {

void
assert_failed(
    char const* msg,
    char const* file,
    std::uint_least32_t line)
{
    llvm::errs() << fmt::format(
        "assertion failed: {} on line {} in {}\n",
        msg, line, ::SourceFileNames::getFileName(file));
}

} // mrdocs
} // clang
