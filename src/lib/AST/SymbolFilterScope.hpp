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

#ifndef MRDOCS_LIB_AST_SYMBOLFILTERSCOPE_HPP
#define MRDOCS_LIB_AST_SYMBOLFILTERSCOPE_HPP

#include "lib/AST/SymbolFilter.hpp"

namespace clang {
namespace mrdocs {

/** Scope for symbol filtering

    This class is used to scope the symbol filter state
    during the traversal of the AST.

    It stores the state of the filter before entering
    a scope and restores it when leaving the scope, after
    the traversal of that scope is complete.
 */
class SymbolFilterScope
{
    SymbolFilter& filter_;
    FilterNode const* current_prev_;
    FilterNode const* last_explicit_prev_;
    bool detached_prev_;

public:
    explicit
    SymbolFilterScope(SymbolFilter& filter)
        : filter_(filter)
        , current_prev_(filter.current)
        , last_explicit_prev_(filter.last_explicit)
        , detached_prev_(filter.detached)
    {
    }

    ~SymbolFilterScope()
    {
        filter_.current = current_prev_;
        filter_.last_explicit = last_explicit_prev_;
        filter_.detached = detached_prev_;
    }
};

} // mrdocs
} // clang

#endif
