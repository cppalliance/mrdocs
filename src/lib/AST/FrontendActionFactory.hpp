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

#include <lib/AST/MissingSymbolSink.hpp>
#include <lib/ConfigImpl.hpp>
#include <lib/Support/ExecutionContext.hpp>
#include <clang/Tooling/Tooling.h>

namespace clang {
namespace mrdocs {

class ASTActionFactory :
    public tooling::FrontendActionFactory
{
    ExecutionContext& ex_;
    ConfigImpl const& config_;
    MissingSymbolSink& missingSink_;
public:
    ASTActionFactory(
        ExecutionContext& ex,
        ConfigImpl const& config,
        MissingSymbolSink& missingSink) noexcept
        : ex_(ex)
        , config_(config)
        , missingSink_(missingSink)
    {
    }

    std::unique_ptr<FrontendAction>
    create() override;
};

} // mrdocs
} // clang

#endif // MRDOCS_LIB_AST_FRONTENDACTIONFACTORY_HPP
