//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_FUNDAMENTALTYPEKIND_HPP
#define MRDOCS_API_METADATA_TYPE_FUNDAMENTALTYPEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <string_view>

namespace clang::mrdocs {

/** Categorically describes a fundamental type.

    @see https://en.cppreference.com/w/cpp/language/types
*/
enum class FundamentalTypeKind
{
    /// void
    Void,
    /// std::nullptr_t
    Nullptr,
    /// bool
    Bool,
    /// char
    Char,
    /// signed char
    SignedChar,
    /// unsigned char
    UnsignedChar,
    /// char8_t
    Char8,
    /// char16_t
    Char16,
    /// char32_t
    Char32,
    /// wchar_t
    WChar,
    /// short / short int / signed short / signed short int
    Short,
    /// unsigned short / unsigned short int
    UnsignedShort,
    /// int / signed / signed int
    Int,
    /// unsigned / unsigned int
    UnsignedInt,
    /// long / long int / signed long / signed long int
    Long,
    /// unsigned long / unsigned long int
    UnsignedLong,
    /// long long / long long int / signed long long / signed long long int
    LongLong,
    /// unsigned long long / unsigned long long int
    UnsignedLongLong,
    /// float
    Float,
    /// double
    Double,
    /// long double
    LongDouble
};

/** Convert a FundamentalTypeKind to a string.

    This function converts a FundamentalTypeKind to
    the shortest canonical string representing the type.

    @return The string representation of the kind
 */
MRDOCS_DECL
std::string_view
toString(FundamentalTypeKind kind) noexcept;

/** Convert a string to a FundamentalTypeKind.

    This function converts a string to a FundamentalTypeKind.

    All variations of the type specifiers are supported.

    However, the "long long" specifier cannot be split
    into two separate specifiers.

    @param str The string to convert
    @param[out] kind The resulting FundamentalTypeKind

    @return true if the string was successfully converted
 */
MRDOCS_DECL
bool
fromString(std::string_view str, FundamentalTypeKind& kind) noexcept;

/** Apply the "long" specifier to the type

    If applying "long" the specifier is a valid operation
    the function changes the type and returns true.

    For instance, applying "long" to
    `FundamentalTypeKind::Int` ("int") results in
    `FundamentalTypeKind::Long` ("long int").

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeLong(FundamentalTypeKind& kind) noexcept;

/** Apply the "short" specifier to the type

    If applying "short" the specifier is a valid operation
    the function changes the type and returns true.

    For instance, applying "short" to
    `FundamentalTypeKind::Int` ("int") results in
    `FundamentalTypeKind::Short` ("short int").

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeShort(FundamentalTypeKind& kind) noexcept;

/** Apply the "signed" specifier to the type

    If applying the "signed" specifier is a valid operation
    the function changes the type and returns true.

    For instance, applying "signed" to
    `FundamentalTypeKind::Char` ("char") results in
    `FundamentalTypeKind::SignedChar` ("signed char").

    It also returns true if applying the "signed" specifier
    is a valid operation but doesn't affect the
    type.

    For instance, applying "signed" to
    `FundamentalTypeKind::Int` ("int") doesn't change the type
    but returns `true`, even though `FundamentalTypeKind::Int`
    could be declared as "int" or "signed" and multiple
    "signed" specifiers are not allowed.

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeSigned(FundamentalTypeKind& kind) noexcept;

/** Apply the "unsigned" specifier to the type

    If applying the "unsigned" specifier is a valid operation
    the function changes the type and returns true.

    For instance, applying "unsigned" to
    `FundamentalTypeKind::Char` ("char") results in
    `FundamentalTypeKind::UnsignedChar` ("unsigned char")
    and applying "unsigned" to
    `FundamentalTypeKind::Int` ("int") results in
    `FundamentalTypeKind::UnsignedInt` ("unsigned int").

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeUnsigned(FundamentalTypeKind& kind) noexcept;

/** Apply the "char" specifier to the type

    If applying the "char" specifier to a type
    that might have been declared only with "signed/unsigned"
    or "short/long" specifiers, the function changes the type
    and returns true.

    For instance, applying "char" to
    `FundamentalTypeKind::Int` ("int", which could be declared
    as "signed") results in
    `FundamentalTypeKind::SignedChar` ("signed char").

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeChar(FundamentalTypeKind& kind) noexcept;

} // clang::mrdocs

#endif
