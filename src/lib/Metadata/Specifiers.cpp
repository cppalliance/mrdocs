//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "mrdocs/Support/Algorithm.hpp"
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

dom::String toString(AttributeKind kind) noexcept
{
    switch(kind)
    {
    case AttributeKind::Deprecated:      return "deprecated";
    case AttributeKind::MaybeUnused:     return "maybe_unused";
    case AttributeKind::Nodiscard:       return "nodiscard";
    case AttributeKind::Noreturn:        return "noreturn";
    case AttributeKind::NoUniqueAddress: return "no_unique_address";
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

bool
isUnaryOperator(OperatorKind kind) noexcept
{
    switch (kind)
    {
    case OperatorKind::Plus:
    case OperatorKind::Minus:
    case OperatorKind::Star:
    case OperatorKind::Amp:
    case OperatorKind::Tilde:
    case OperatorKind::Exclaim:
    case OperatorKind::PlusPlus:
    case OperatorKind::MinusMinus:
    case OperatorKind::New:
    case OperatorKind::Delete:
    case OperatorKind::ArrayNew:
    case OperatorKind::ArrayDelete:
    case OperatorKind::Coawait:
        return true;
    default:
        return false;
    }
}

bool
isBinaryOperator(OperatorKind kind) noexcept
{
    switch (kind)
    {
    case OperatorKind::Plus:
    case OperatorKind::Minus:
    case OperatorKind::Star:
    case OperatorKind::Slash:
    case OperatorKind::Percent:
    case OperatorKind::Caret:
    case OperatorKind::Amp:
    case OperatorKind::Pipe:
    case OperatorKind::LessLess:
    case OperatorKind::GreaterGreater:
    case OperatorKind::Equal:
    case OperatorKind::PlusEqual:
    case OperatorKind::MinusEqual:
    case OperatorKind::StarEqual:
    case OperatorKind::SlashEqual:
    case OperatorKind::PercentEqual:
    case OperatorKind::CaretEqual:
    case OperatorKind::AmpEqual:
    case OperatorKind::PipeEqual:
    case OperatorKind::LessLessEqual:
    case OperatorKind::GreaterGreaterEqual:
    case OperatorKind::EqualEqual:
    case OperatorKind::ExclaimEqual:
    case OperatorKind::Less:
    case OperatorKind::LessEqual:
    case OperatorKind::Greater:
    case OperatorKind::GreaterEqual:
    case OperatorKind::Spaceship:
    case OperatorKind::AmpAmp:
    case OperatorKind::PipePipe:
    case OperatorKind::ArrowStar:
    case OperatorKind::Arrow:
    case OperatorKind::Call:
    case OperatorKind::Subscript:
    case OperatorKind::Comma:
        return true;
    default:
        return false;
    }
}


} // clang
} // mrdocs
