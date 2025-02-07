//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Specifiers.hpp>

namespace clang {
namespace mrdocs {

dom::String toString(AccessKind kind) noexcept
{
    switch(kind)
    {
    case AccessKind::Public:    return "public";
    case AccessKind::Private:   return "private";
    case AccessKind::Protected: return "protected";
    case AccessKind::None:      return "";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String toString(StorageClassKind kind) noexcept
{
    switch(kind)
    {
    case StorageClassKind::None:     return "";
    case StorageClassKind::Extern:   return "extern";
    case StorageClassKind::Static:   return "static";
    case StorageClassKind::Auto:     return "auto";
    case StorageClassKind::Register: return "register";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String toString(ConstexprKind kind) noexcept
{
    switch(kind)
    {
    case ConstexprKind::None:      return "";
    case ConstexprKind::Constexpr: return "constexpr";
    case ConstexprKind::Consteval: return "consteval";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String toString(ExplicitKind kind) noexcept
{
    switch(kind)
    {
    case ExplicitKind::False:     return "";
    case ExplicitKind::True:      return "explicit";
    case ExplicitKind::Dependent: return "explicit(...)";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String toString(NoexceptKind kind) noexcept
{
    switch(kind)
    {
    case NoexceptKind::False:     return "";
    case NoexceptKind::True:      return "noexcept";
    case NoexceptKind::Dependent: return "noexcept(...)";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(
    NoexceptInfo const& info,
    bool resolved,
    bool implicit)
{
    if(! implicit && info.Implicit)
        return "";
    if(info.Kind == NoexceptKind::Dependent &&
        info.Operand.empty())
        return "";
    if(info.Kind == NoexceptKind::False &&
        (resolved || info.Operand.empty()))
        return "";
    if(info.Kind == NoexceptKind::True &&
        (resolved || info.Operand.empty()))
        return "noexcept";
    return fmt::format("noexcept({})", info.Operand);
}

dom::String
toString(
    ExplicitInfo const& info,
    bool resolved,
    bool implicit)
{
    if(! implicit && info.Implicit)
        return "";
    if(info.Kind == ExplicitKind::Dependent &&
        info.Operand.empty())
        return "";
    if(info.Kind == ExplicitKind::False &&
        (resolved || info.Operand.empty()))
        return "";
    if(info.Kind == ExplicitKind::True &&
        (resolved || info.Operand.empty()))
        return "explicit";
    return fmt::format("explicit({})", info.Operand);
}

dom::String toString(ReferenceKind kind) noexcept
{
    switch(kind)
    {
    case ReferenceKind::None:   return "";
    case ReferenceKind::LValue: return "&";
    case ReferenceKind::RValue: return "&&";
    default:
        MRDOCS_UNREACHABLE();
    }
}

} // clang
} // mrdocs
