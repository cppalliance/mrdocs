//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_NOOP_NOOPGENERATOR_HPP
#define MRDOCS_LIB_GEN_NOOP_NOOPGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Generator.hpp>

namespace mrdocs::noop {

/** A generator that extracts but produces no output.

    The no-op generator runs the full extraction phase and reports any
    diagnostics, then discards the result without writing any files. It
    is useful for checking that extraction succeeds and for surfacing
    extraction warnings without committing an expected output.
*/
struct NoopGenerator : Generator
{
    std::string_view
    id() const noexcept override
    {
        return "noop";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "No-op (extraction only)";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        // The empty extension signals that no output file is produced.
        return "";
    }

    Expected<void>
    build(Corpus const& corpus, Config const& config) const override;
};

} // mrdocs::noop

#endif
