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

#include "ASTVisitorConsumer.hpp"
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include "ASTVisitor.hpp"

namespace mrdocs {

void
ASTVisitorConsumer::
HandleTranslationUnit(clang::ASTContext& Context)
{
    MRDOCS_ASSERT(sema_);
    Diagnostics diags;
    ASTVisitor visitor(
        config_,
        diags,
        compiler_,
        Context,
        *sema_,
        macroDefs_);
    visitor.build();
    ex_.report(std::move(visitor.results()), std::move(diags), std::move(visitor.undocumented()));
}

} // mrdocs
