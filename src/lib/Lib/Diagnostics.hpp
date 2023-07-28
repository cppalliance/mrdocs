//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_TOOL_DIAGNOSTICS_HPP
#define MRDOX_LIB_TOOL_DIAGNOSTICS_HPP

#include <mrdox/Support/Error.hpp>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <unordered_map>

namespace clang {
namespace mrdox {

/** Diagnostic information accumulated during visitation.
*/
class Diagnostics
{
    std::size_t errorCount_ = 0;
    std::unordered_map<std::string, bool> messages_;

public:
    void error(std::string s)
    {
        auto result =
            messages_.emplace(std::move(s), true);
        if(result.second)
            ++errorCount_;
    }

    void warn(std::string s)
    {
        messages_.emplace(std::move(s), false);
    }

    void reportTotals(report::Level level)
    {
        std::string s;
        if(! messages_.empty())
        {
            auto warnCount = messages_.size() - errorCount_;
            if(errorCount_ > 0)
            {
                fmt::format_to(std::back_inserter(s),
                    "{} {}",
                    errorCount_, errorCount_ > 1
                        ? "errors" : "error");
            }
            if(warnCount > 0)
            {
                if(errorCount_ > 0)
                    fmt::format_to(std::back_inserter(s),
                        " and " );
                fmt::format_to(std::back_inserter(s),
                    "{} {}.\n",
                    warnCount, warnCount > 1
                        ? "warnings" : "warning");
            }
        }
        else
        {
            fmt::format_to(std::back_inserter(s),
                "No errors or warnings.\n");
        }
        report::print(level, s);
    }

    void
    mergeAndReport(
        Diagnostics&& other)
    {
        for(auto&& m : other.messages_)
        {
            auto result = messages_.emplace(std::move(m));
            if(result.second)
            {
                if(result.first->second)
                    ++errorCount_;
                auto const level = result.first->second
                    ? report::Level::error
                    : report::Level::warn;
                report::print(
                    level, result.first->first);
            }
        }
        other.messages_.clear();
    }
};

} // mrdox
} // clang

#endif
