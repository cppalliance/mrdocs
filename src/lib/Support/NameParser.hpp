//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_NAMEPARSER_HPP
#define MRDOCS_LIB_SUPPORT_NAMEPARSER_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace clang {
namespace mrdocs {

struct ParseResult
{
    bool qualified = false;
    std::vector<std::string> qualifier;
    std::string name;
};

Expected<ParseResult>
parseIdExpression(
    std::string_view str,
    bool allow_wildcards = false);

} // mrdocs
} // clang

#endif
