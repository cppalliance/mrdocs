//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_ASTVISITOR_HPP
#define MRDOCS_LIB_AST_ASTVISITOR_HPP

#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/ExecutionContext.hpp"
#include <mrdocs/Platform.hpp>
#include <clang/Tooling/Tooling.h>

namespace clang {
namespace mrdocs {

/** Return a factory used to create our visitor.
*/
std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    std::string_view argv0,
    ExecutionContext& ex,
    ConfigImpl const& config);

} // mrdocs
} // clang

#endif
