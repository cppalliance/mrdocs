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

#include "Operator.hpp"
#include <llvm/Support/ErrorHandling.h>

namespace clang {
namespace mrdox {

llvm::StringRef
getSafeOperatorName(
    OverloadedOperatorKind OOK) noexcept
{
    switch(OOK)
    {
    case OO_None:                return "";
    case OO_New:                 return "new";      // new
    case OO_Delete:              return "del";      // delete
    case OO_Array_New:           return "arr_new";  // new[]
    case OO_Array_Delete:        return "arr_del";  // delete[]
    case OO_Plus:                return "plus";     // +
    case OO_Minus:               return "minus";    // -
    case OO_Star:                return "star";     // *
    case OO_Slash:               return "slash";    // /
    case OO_Percent:             return "mod";      // %
    case OO_Caret:               return "xor";      // ^
    case OO_Amp:                 return "bitand";   // &      
    case OO_Pipe:                return "bitor";    // |
    case OO_Tilde:               return "bitnot";   // ~
    case OO_Exclaim:             return "not";      // !
    case OO_Equal:               return "assign";   // =
    case OO_Less:                return "lt";       // <
    case OO_Greater:             return "gt";       // >
    case OO_PlusEqual:           return "plus_eq";  // +=
    case OO_MinusEqual:          return "minus_eq"; // -=
    case OO_StarEqual:           return "star_eq";  // *=
    case OO_SlashEqual:          return "slash_eq"; // /=
    case OO_PercentEqual:        return "mod_eq";   // %=
    case OO_CaretEqual:          return "xor_eq";   // ^=
    case OO_AmpEqual:            return "and_eq";   // &=
    case OO_PipeEqual:           return "or_eq";    // |=
    case OO_LessLess:            return "lt_lt";    // <<
    case OO_GreaterGreater:      return "gt_gt";    // >>
    case OO_LessLessEqual:       return "lt_lt_eq"; // <<=
    case OO_GreaterGreaterEqual: return "gt_gt_eq"; // >>=
    case OO_EqualEqual:          return "eq";       // ==
    case OO_ExclaimEqual:        return "not_eq";   // !=
    case OO_LessEqual:           return "le";       // <=
    case OO_GreaterEqual:        return "ge";       // >=
    case OO_Spaceship:           return "3way";     // <=>
    case OO_AmpAmp:              return "and";      // &&
    case OO_PipePipe:            return "or";       // ||
    case OO_PlusPlus:            return "inc";      // ++
    case OO_MinusMinus:          return "dec";      // --
    case OO_Comma:               return "comma";    // ,
    case OO_ArrowStar:           return "ptrmem";   // ->*
    case OO_Arrow:               return "ptr";      // ->
    case OO_Call:                return "call";     // ()
    case OO_Subscript:           return "subs";     // []
    case OO_Conditional:         return "ternary";  // ?
    case OO_Coawait:             return "coawait";  // co_await
    default:
        llvm_unreachable("unknown op");
    };
}

} // mrdox
} // clang
