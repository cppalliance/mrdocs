//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_USING_HPP
#define MRDOCS_API_METADATA_USING_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <vector>
#include <utility>

namespace clang {
namespace mrdocs {

enum class UsingClass
{
    Normal = 0,     // using
    Typename,       // using typename
    Enum,           // using enum
    Namespace       // using namespace (using directive)
};

static constexpr
std::string_view
toString(UsingClass const& value)
{
    switch (value)
    {
    case UsingClass::Normal:    return "normal";
    case UsingClass::Typename:  return "typename";
    case UsingClass::Enum:      return "enum";
    case UsingClass::Namespace: return "namespace";
    }
    return "unknown";
}

/** Info for using declarations and directives.
 */
struct UsingInfo
    : IsInfo<InfoKind::Using>,
    SourceInfo
{
    /** The kind of using declaration/directive. */
    UsingClass Class = UsingClass::Normal;

    /** The symbols being "used". */
    std::vector<SymbolID> UsingSymbols;

    /** The qualifier for a using declaration/directive. */
    std::unique_ptr<NameInfo> Qualifier;

    //--------------------------------------------

    explicit UsingInfo(SymbolID ID) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
