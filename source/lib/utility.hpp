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

#ifndef MRDOX_SOURCE_UTILITY_HPP
#define MRDOX_SOURCE_UTILITY_HPP

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

/// Replaces backslashes with slashes if Windows in place.
///
/// @param path A path that is transformed to native format.
/// On Unix, this function is a no-op because backslashes
/// are valid path chracters.
llvm::StringRef
convert_to_slash(
    llvm::SmallVectorImpl<char> &path,
    llvm::sys::path::Style style =
        llvm::sys::path::Style::native);

/** Append a separator if not already present.
*/
void
makeDirsy(
    llvm::SmallVectorImpl<char>& s);

} // mrdox
} // clang

#endif
