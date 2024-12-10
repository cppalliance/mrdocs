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

#ifndef MRDOCS_LIB_AST_SYMBOLFILTER_HPP
#define MRDOCS_LIB_AST_SYMBOLFILTER_HPP

#include "lib/Lib/Filters.hpp"

namespace clang {
namespace mrdocs {

/** Filter for symbols

    This class is used to filter symbols based on the
    configuration provided by the user.

 */
struct SymbolFilter
{
    const FilterNode& root;
    const FilterNode* current = nullptr;
    const FilterNode* last_explicit = nullptr;
    bool detached = false;

    SymbolFilter(const FilterNode& root_node)
        : root(root_node)
    {
        setCurrent(&root, false);
    }

    SymbolFilter(const SymbolFilter&) = delete;
    SymbolFilter(SymbolFilter&&) = delete;

    void
    setCurrent(
        const FilterNode* node,
        bool node_detached)
    {
        current = node;
        detached = node_detached;
        if(node && node->Explicit)
            last_explicit = node;
    }
};

/** Scope for symbol filtering

    This class is used to scope the symbol filter state
    during the traversal of the AST.

    It stores the state of the filter before entering
    a scope and restores it when leaving the scope, after
    the traversal of that scope is complete.
 */
class FilterScope
{
    SymbolFilter& filter_;
    FilterNode const* current_prev_;
    FilterNode const* last_explicit_prev_;
    bool detached_prev_;

public:
    explicit
    FilterScope(SymbolFilter& filter)
        : filter_(filter)
        , current_prev_(filter.current)
        , last_explicit_prev_(filter.last_explicit)
        , detached_prev_(filter.detached)
    {
    }

    ~FilterScope()
    {
        filter_.current = current_prev_;
        filter_.last_explicit = last_explicit_prev_;
        filter_.detached = detached_prev_;
    }
};

} // mrdocs
} // clang

#endif
