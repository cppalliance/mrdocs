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

#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>

namespace mrdocs {

GeneratorExtensionContext
makeGeneratorExtensionContext(
    Generator const& generator,
    Corpus const& corpus,
    Config const& config,
    report::Reporter& reporter)
{
    GeneratorExtensionContext context;
    context.corpus = &corpus;
    context.config = &config;
    context.outputPath = getGeneratorOutputPath(generator, config);
    context.params = getGeneratorParams(generator, config);
    context.report = &reporter;
    return context;
}

} // mrdocs
