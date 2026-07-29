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

#ifndef MRDOCS_LIB_GEN_HBS_DATADRIVENGENERATORS_HPP
#define MRDOCS_LIB_GEN_HBS_DATADRIVENGENERATORS_HPP

#include "HandlebarsGenerator.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <string_view>

// `discoverDataDrivenGenerators` is part of the public generator API; see
// its declaration in <mrdocs/Generator.hpp>.

namespace mrdocs::hbs {

/** Load mrdocs-generator.yml and return the resulting `EscapeMap`.

    A thin convenience over `loadGeneratorManifest` (see
    <lib/Gen/GeneratorManifest.hpp>) that keeps only the escape rules,
    for callers that render output and don't need the other manifest
    fields. Parsing rules and errors are as documented there.
*/
Expected<EscapeMap>
loadGeneratorMetadata(std::string_view yamlPath);

} // namespace mrdocs::hbs

#endif
