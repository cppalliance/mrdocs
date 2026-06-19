//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "NoopGenerator.hpp"

namespace mrdocs {
namespace noop {

Expected<void>
NoopGenerator::
build(Corpus const&) const
{
    // Extraction has already happened by the time a generator runs;
    // the no-op generator deliberately writes nothing.
    return {};
}

} // noop

//------------------------------------------------

std::unique_ptr<Generator>
makeNoopGenerator()
{
    return std::make_unique<noop::NoopGenerator>();
}

} // mrdocs
