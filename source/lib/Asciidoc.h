//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_ASCIIDOC_HPP
#define MRDOX_ASCIIDOC_HPP

namespace mrdox {
namespace adoc {

template<class T>
class table
{
    T const& t_;

public:
    explicit
    table(T const& t) noexcept
        : t_(t)
    {
    }

    template<class T_>
    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        table<T_> const& t)
    {
        os <<
            "[cols=2]\n" <<
            "|===\n";

        if(t.empty())
        {
            os <<
                "|===\n"
                "\n";
            return os;
        }
    }
};

} // adoc
} // mrdox

#endif
