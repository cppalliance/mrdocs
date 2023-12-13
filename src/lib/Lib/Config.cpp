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

#include "mrdocs/Config.hpp"
#include <mrdocs/Support/Error.hpp>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/FileSystem.h>

#include <version>

// Check llvm version
#define STRINGIFY(Value) #Value
static_assert(
    LLVM_VERSION_MAJOR >= MRDOCS_MINIMUM_LLVM_VERSION,
    "MrDocs requires at least clang " STRINGIFY(MRDOCS_MINIMUM_LLVM_VERSION)
    ", got " LLVM_VERSION_STRING " instead.");

#if ! defined(__cpp_lib_ranges)
#error "Ranges library unavailable"
#endif

#include <ranges>
#include <fmt/format.h>

namespace clang {
namespace mrdocs {

Config::
Config() noexcept
{
}

Config::
~Config() noexcept = default;

} // mrdocs
} // clang
