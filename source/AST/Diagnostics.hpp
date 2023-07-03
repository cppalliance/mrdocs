//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_AST_DIAGNOSTICS_HPP
#define MRDOX_TOOL_AST_DIAGNOSTICS_HPP

#include <mrdox/Support/Error.hpp>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <unordered_set>

namespace clang {
namespace mrdox {

/** Diagnostic information accumulated during visitation.
*/
class Diagnostics
{
    std::size_t errorCount_ = 0;
    std::unordered_set<std::string> messages_;

public:
    void reportError(std::string s)
    {
        auto result =
            messages_.emplace(std::move(s));
        if(result.second)
            ++errorCount_;
    }

    void reportWarning(std::string s)
    {
        auto result =
            messages_.emplace(std::move(s));
        if(result.second)
            ++errorCount_;
    }

    void
    merge(
        Diagnostics&& other,
        llvm::raw_ostream* os = nullptr)
    {
        for(auto&& s : other.messages_)
        {
            auto result = messages_.emplace(std::move(s));
            if(os && result.second)
                *os << *result.first;
        }
        other.messages_.clear();
    }
};

} // mrdox
} // clang

#endif
