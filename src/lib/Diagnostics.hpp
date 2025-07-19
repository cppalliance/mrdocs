//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_TOOL_DIAGNOSTICS_HPP
#define MRDOCS_LIB_TOOL_DIAGNOSTICS_HPP

#include <mrdocs/Support/Report.hpp>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <unordered_map>

namespace clang {
namespace mrdocs {

/** Diagnostic information accumulated during visitation.

    The Diagnostics class is used to accumulate
    diagnostic information during visitation.

    The information is accumulated in a map of
    strings, where each string is a unique message.

    Each message can be a warning or an error.

*/
class Diagnostics
{
    std::size_t errorCount_ = 0;
    std::unordered_map<std::string, bool> messages_;

public:
    /** Add an error message to the diagnostics.

        This function adds an error message to the
        accumulated diagnostics.

        @param s The error message to add.
    */
    void error(std::string s)
    {
        if (messages_.emplace(std::move(s), true).second)
        {
            ++errorCount_;
        }
    }

    /** Add a warning message to the diagnostics.

        This function adds a warning message to the
        accumulated diagnostics.

        @param s The warning message to add.
    */
    void warn(std::string s)
    {
        messages_.emplace(std::move(s), false);
    }

    /** Print the accumulated diagnostics.

        This function prints the accumulated diagnostics
        to the output stream.

        It will print a summary of the number of errors
        and warnings.

        @param level The level of the report.
        @param os The output stream to write to.
    */
    void
    reportTotals(report::Level level)
    {
        if (messages_.empty())
        {
            return;
        }

        std::string s;
        auto const warnCount = messages_.size() - errorCount_;
        if (errorCount_ > 0)
        {
            fmt::format_to(
                std::back_inserter(s),
                "{} error{}", errorCount_,
                errorCount_ > 1 ? "s" : "");
        }
        if (warnCount > 0)
        {
            if(errorCount_ > 0)
            {
                fmt::format_to(std::back_inserter(s), " and " );
            }
            fmt::format_to(
                std::back_inserter(s),
                "{} warning{}.\n", warnCount,
                warnCount > 1 ? "s" : "");
        }
        fmt::format_to(std::back_inserter(s), " total.");
        report::print(level, s);
    }

    /** Merge diagnostics from another object and print new messages.

        This function merges the diagnostics from
        another object into this one.

        For each new message that is added, it is printed
        to the output stream if it's a new message.

        @param other The other diagnostics to merge.
    */
    void
    mergeAndReport(
        Diagnostics&& other)
    {
        for(auto&& m : other.messages_)
        {
            auto [it, ok] = messages_.emplace(std::move(m));
            if (ok)
            {
                auto const& [msg, is_error] = *it;
                if (is_error)
                {
                    ++errorCount_;
                }
                auto const level = is_error
                    ? report::Level::error
                    : report::Level::warn;
                report::print(level, msg);
            }
        }
        other.messages_.clear();
        other.errorCount_ = 0;
    }
};

} // mrdocs
} // clang

#endif
