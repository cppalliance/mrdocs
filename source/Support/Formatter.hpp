//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_FORMATTER_HPP
#define MRDOX_API_SUPPORT_FORMATTER_HPP

#include <mrdox/Platform.hpp>
#include <llvm/ADT/SmallString.h>
#include <charconv>
#include <concepts>
#include <string_view>
#include <type_traits>

/*  This API offers formatted output tailored to
    the needs of generators that emit text.
*/

namespace clang {
namespace mrdox {

/** Produces formatted output to a stream.
*/
template<class Output>
class Formatter
{
    Output& os_;
    std::string indent_;
    bool needIndent_ = true;

public:
    /** Constructor.
    */
    explicit
    Formatter(
        Output& os) noexcept
        : os_(os)
    {
    }

    /** Set the indentation level.

        @return The previous indentation level.
    */
    std::size_t indent(int n)
    {
        auto const n0 = indent_.size();
        indent_.resize(n0 + n, ' ');
        return n0;
    }

    /** Write a line of formatted output.

        This function efficiently converts each
        argument to a string one at a time and
        writes them to the output. After all the
        arguments are written, a newline is emitted.

        The string will be printed at the current
        indentation level.
    */
    template<class Arg0, class... ArgN>
    void
    operator()(Arg0&& arg0, ArgN&&... argn)
    {
        if constexpr(sizeof...(argn) == 0)
        {
            write(std::forward<Arg0>(arg0));
        }
        else
        {
            write(std::forward<Arg0>(arg0));
            (*this)(std::forward<ArgN>(argn)...);
        }
    }

    void operator()() const noexcept = delete;

private:
    // emit a newline
    void write_newline()
    {
        os_.write("\n", 1);
    }

    // write the string to the stream
    void write_impl(std::string_view s)
    {
        auto it = s.data();
        auto const end = it + s.size();
        auto const flush =
            [this](auto it0, auto it)
            {
                if(needIndent_)
                {
                    os_.write(indent_.data(), indent_.size());
                    needIndent_ = false;
                }
                os_.write(it0, it - it0);
            };
        for(;;)
        {
            if(it == end)
                break;
            if(*it != '\n')
            {
            do_chars:
                auto it0 = it;
                for(;;)
                {
                    ++it;
                    if(it == end)
                        return flush(it0, it);
                    if(*it != '\n')
                        continue;
                    flush(it0, it);
                    break;
                }
            }
            needIndent_ = true;
            for(;;)
            {
                write_newline();
                ++it;
                if(it == end)
                    return;
                if(*it == '\n')
                    continue;
                goto do_chars;
            }
        }
    }

    // string literal
    void write(char const* sz)
    {
        write_impl(std::string_view(sz));
    }

    template<std::size_t N>
    void write(llvm::SmallString<N> const& ss)
    {
        write_impl(std::string_view(ss.data(), ss.size()));
    }

    void write(std::string const& s)
    {
        write_impl(std::string_view(s.data(), s.size()));
    }

    // convertible to std::string_view
    template<class T>
    requires
        std::is_convertible_v<T, std::string_view>
    void write(T const& t)
    {
        write_impl(std::string_view(t));
    }

    // passed to std::to_chars
    template<typename N>
    requires
        requires(N n) { std::to_chars(n); }
    void write(N n)
    {
        write_impl(std::to_chars(n));
    }
};

} // mrdox
} // clang

#endif
