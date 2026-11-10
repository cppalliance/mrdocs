//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "BuiltinGenerators.hpp"
#include <mrdocs/Extensions/GeneratorRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <memory>

namespace mrdocs {

// Each built-in generator defines its own factory next to its class.
std::unique_ptr<Generator> makeAdocGenerator();
std::unique_ptr<Generator> makeXMLGenerator();
std::unique_ptr<Generator> makeJSONGenerator();
std::unique_ptr<Generator> makeHTMLGenerator();
std::unique_ptr<Generator> makeNoopGenerator();

Expected<void>
registerBuiltinGenerators(GeneratorRegistry& registry)
{
    MRDOCS_TRY(registry.install(makeAdocGenerator()));
    MRDOCS_TRY(registry.install(makeXMLGenerator()));
    MRDOCS_TRY(registry.install(makeJSONGenerator()));
    MRDOCS_TRY(registry.install(makeHTMLGenerator()));
    MRDOCS_TRY(registry.install(makeNoopGenerator()));
    return {};
}

} // mrdocs
