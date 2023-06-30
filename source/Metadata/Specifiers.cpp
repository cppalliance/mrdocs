//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Metadata/Specifiers.hpp>

namespace clang {
namespace mrdox {

std::string_view
toString(AccessKind kind)
{
    switch(kind)
    {
    case AccessKind::Public:    return "public";
    case AccessKind::Private:   return "private";
    case AccessKind::Protected: return "protected";
    case AccessKind::None:      return "";
    default:
        MRDOX_UNREACHABLE();
    }
}

std::string_view
toString(StorageClassKind kind)
{
    switch(kind)
    {
    case StorageClassKind::None:     return "";
    case StorageClassKind::Extern:   return "extern";
    case StorageClassKind::Static:   return "static";
    case StorageClassKind::Auto:     return "auto";
    case StorageClassKind::Register: return "register";
    default:
        MRDOX_UNREACHABLE();
    }
}

std::string_view
toString(ConstexprKind kind)
{
    switch(kind)
    {
    case ConstexprKind::None:      return "";
    case ConstexprKind::Constexpr: return "constexpr";
    case ConstexprKind::Consteval: return "consteval";
    default:
        MRDOX_UNREACHABLE();
    }
}

std::string_view
toString(ExplicitKind kind)
{
    switch(kind)
    {
    case ExplicitKind::None:               return "";
    case ExplicitKind::Explicit:           return "explicit";
    case ExplicitKind::ExplicitFalse:      return "explicit(false)";
    case ExplicitKind::ExplicitTrue:       return "explicit(true)";
    case ExplicitKind::ExplicitUnresolved: return "explicit(expr)";
    default:
        MRDOX_UNREACHABLE();
    }
}

std::string_view
toString(NoexceptKind kind)
{
    switch(kind)
    {
    case NoexceptKind::None:              return "";
    case NoexceptKind::ThrowNone:         return "throw()";
    case NoexceptKind::Throw:             return "throw";
    case NoexceptKind::ThrowAny:          return "throw(...)";
    case NoexceptKind::NoThrow:           return "__declspec(nothrow)";
    case NoexceptKind::Noexcept:          return "noexcept";
    case NoexceptKind::NoexceptFalse:     return "noexcept(false)";
    case NoexceptKind::NoexceptTrue:      return "noexcept(true)";
    case NoexceptKind::NoexceptDependent: return "noexcept(expr)";
    case NoexceptKind::Unevaluated:       return "";
    case NoexceptKind::Uninstantiated:    return "";
    case NoexceptKind::Unparsed:          return "";
    default:
        MRDOX_UNREACHABLE();
    }
}

std::string_view
toString(ReferenceKind kind)
{
    switch(kind)
    {
    case ReferenceKind::None:   return "";
    case ReferenceKind::LValue: return "&";
    case ReferenceKind::RValue: return "&&";
    default:
        MRDOX_UNREACHABLE();
    }
}

} // clang
} // mrdox
