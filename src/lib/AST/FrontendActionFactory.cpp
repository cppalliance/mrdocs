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

#include "FrontendActionFactory.hpp"
#include <lib/AST/ASTAction.hpp>

namespace clang {
namespace mrdocs {


std::unique_ptr<FrontendAction>
ASTActionFactory::
create()
{
    auto A = std::make_unique<ASTAction>(ex_, config_);
    A->setMissingSymbolSink(missingSink_);
    return A;
}

} // mrdocs
} // clang
