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
#include <mrdocs/Support/Algorithm.hpp>
#include <format>


namespace mrdocs {

std::string
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
    return std::format("noexcept({})", info.Operand);
}

std::string
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
    return std::format("explicit({})", info.Operand);
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



} // mrdocs
