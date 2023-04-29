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

#ifndef MRDOX_METADATA_FUNCTIONKIND_HPP
#define MRDOX_METADATA_FUNCTIONKIND_HPP

#include <mrdox/Platform.hpp>

namespace clang {
namespace mrdox {

/** An enumeration of the different kinds of functions.
*/
enum class FunctionKind
{
    // VFALCO The most frequent function kind should be
    // here, since the bitstream writer does not emit zeroes.
    Plain = 0,

    // The operator kind values have to match the clang enumeration
    OpNew /* = clang::OverloadedOperatorKind::OO_New */,
    OpDelete,
    OpArray_New,
    OpArray_Delete,
    OpPlus,
    OpMinus,
    OpStar,
    OpSlash,
    OpPercent,
    OpCaret,
    OpAmp,
    OpPipe,
    OpTilde,
    OpExclaim,
    OpEqual,
    OpLess,
    OpGreater,
    OpPlusEqual,
    OpMinusEqual,
    OpStarEqual,
    OpSlashEqual,
    OpPercentEqual,
    OpCaretEqual,
    OpAmpEqual,
    OpPipeEqual,
    OpLessLess,
    OpGreaterGreater,
    OpLessLessEqual,
    OpGreaterGreaterEqual,
    OpEqualEqual,
    OpExclaimEqual,
    OpLessEqual,
    OpGreaterEqual,
    OpSpaceship,
    OpAmpAmp,
    OpPipePipe,
    OpPlusPlus,
    OpMinusMinus,
    OpComma,
    OpArrowStar,
    OpArrow,
    OpCall,
    OpSubscript,
    OpConditional,
    OpCoawait,
    NUM_OVERLOADED_OPERATORS /* = clang::OverloadedOperatorKind::NUM_OVERLOADED_OPERATORS */,

    Destructor = NUM_OVERLOADED_OPERATORS,
    Constructor,
    Conversion
};

} // mrdox
} // clang

#endif
