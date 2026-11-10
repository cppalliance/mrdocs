//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Extensions/TransformExtensionContext.hpp>

namespace mrdocs {

dom::Object
getTransformParams(Transform const& transform, Config const& config)
{
    auto const& options = config.transformOptions;
    auto const it = options.find(std::string(transform.id()));
    return it != options.end() ? it->second : dom::Object();
}

TransformExtensionContext
makeTransformExtensionContext(
    Transform const& transform,
    Corpus& corpus,
    Config const& config,
    report::Reporter& reporter)
{
    TransformExtensionContext context;
    context.corpus = &corpus;
    context.config = &config;
    context.params = getTransformParams(transform, config);
    context.report = &reporter;
    return context;
}

} // mrdocs
