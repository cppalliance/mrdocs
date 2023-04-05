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

//
// This file defines the internal representations of different declaration
// types for the clang-doc tool.
//

#ifndef MRDOX_JAD_ACCESSSCOPE_HPP
#define MRDOX_JAD_ACCESSSCOPE_HPP

#include "jad/ScopeChildren.hpp"
#include <clang/Basic/Specifiers.h>

namespace clang {
namespace mrdox {

/** Children of a class, struct, or union.
*/
struct AccessScope
{
    ScopeChildren& pub;
    ScopeChildren& prot;
    ScopeChildren& priv;

    ScopeChildren&
    get(clang::AccessSpecifier access)
    {
        assert(access != clang::AccessSpecifier::AS_none);
        return v_[access];
    }

    AccessScope() noexcept
        : pub(v_[0])
        , prot(v_[1])
        , priv(v_[2])
        , v_{
            clang::AccessSpecifier::AS_public,
            clang::AccessSpecifier::AS_protected,
            clang::AccessSpecifier::AS_private}
    {
    }

private:
    ScopeChildren v_[3];
};

} // mrdox
} // clang

#endif
