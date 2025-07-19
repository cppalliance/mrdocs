//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_FRONTENDACTIONFACTORY_HPP
#define MRDOCS_LIB_AST_FRONTENDACTIONFACTORY_HPP

#include "lib/ConfigImpl.hpp"
#include "lib/Support/ExecutionContext.hpp"
#include <clang/Tooling/Tooling.h>

namespace clang {
namespace mrdocs {

/** Return a factory of MrDocs actions for the Clang Frontend

    This function returns an implementation of
    `clang::tooling::FrontendActionFactory` that allows
    one action to be created for each translation unit.

    The `create` method of this factory returns a new instance
    of @ref clang::mrdocs::ASTAction for each translation unit.

    A `tooling::ClangTool`, with access to the compilation database,
    can receive this factory action via `tooling::ClangTool::run()`.
    This is the entry point for the AST traversal in `CorpusImpl::build`.
 */
std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    ExecutionContext& ex,
    ConfigImpl const& config);

} // mrdocs
} // clang

#endif
