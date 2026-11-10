//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Platform.hpp>

// The tag sits between brackets so that file(STRINGS) finds it in the object
// code. See mrdocs_detect_stdlib_tag in helpers/mrdocs_detect_stdlib_tag.cmake.
extern "C" char const mrdocs_stdlib_tag[] =
    "MRDOCS_STDLIB_TAG[" MRDOCS_DETAIL_PLUGIN_STDLIB_TAG_VALUE "]";
