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
#include "lib/AST/ASTAction.hpp"

namespace clang {
namespace mrdocs {

std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    ExecutionContext& ex,
    ConfigImpl const& config)
{
    class ASTActionFactory :
        public tooling::FrontendActionFactory
    {
        ExecutionContext& ex_;
        ConfigImpl const& config_;
    public:
        ASTActionFactory(
            ExecutionContext& ex,
            ConfigImpl const& config) noexcept
            : ex_(ex)
            , config_(config)
        {
        }

        std::unique_ptr<FrontendAction>
        create() override
        {
            return std::make_unique<ASTAction>(ex_, config_);
        }
    };

    return std::make_unique<ASTActionFactory>(ex, config);
}

} // mrdocs
} // clang
