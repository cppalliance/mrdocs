//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SPECIFIERS_STORAGECLASSKIND_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_STORAGECLASSKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <string>

namespace clang::mrdocs {

/** Storage class kinds

    [dcl.stc] p1: At most one storage-class-specifier shall appear
    in a given decl-specifier-seq, except that `thread_local`
    may appear with `static` or `extern`.
*/
enum class StorageClassKind
{
    /// No storage class specifier
    None = 0,
    /// thread_local storage-class-specifier
    Extern,
    /// mutable storage-class-specifier
    Static,
    /// auto storage-class-specifier (removed in C++11)
    /// only valid for variables
    Auto,
    /// register storage-class-specifier (removed in C++17)
    /// only valid for variables
    Register
};

MRDOCS_DECL
dom::String
toString(StorageClassKind kind) noexcept;

/** Return the StorageClassKind as a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    StorageClassKind const kind)
{
    v = toString(kind);
}

} // clang::mrdocs

#endif
