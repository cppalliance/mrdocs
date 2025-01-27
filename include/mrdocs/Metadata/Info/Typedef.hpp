//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPEDEF_HPP
#define MRDOCS_API_METADATA_TYPEDEF_HPP

#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

// Info for typedef and using statements.
struct TypedefInfo final
    : InfoCommonBase<InfoKind::Typedef>
{
    PolymorphicValue<TypeInfo> Type;

    /** Indicates if this is a new C++ "using"-style typedef

        @code
        using MyVector = std::vector<int>
        @endcode

        False means it's a C-style typedef:

        @code
        typedef std::vector<int> MyVector;
        @endcode
      */
    bool IsUsing = false;

    std::optional<TemplateInfo> Template;

    //--------------------------------------------

    explicit TypedefInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

} // clang::mrdocs

#endif
