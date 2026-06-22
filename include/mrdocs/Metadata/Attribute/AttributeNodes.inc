//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef INFO
#define INFO(PascalName)
#endif

/// An attribute mrdocs does not treat specially.
INFO(Other)
/// `[[noreturn]]` (C++11)
INFO(Noreturn)
/// `[[carries_dependency]]` (C++11)
INFO(CarriesDependency)
/// `[[deprecated]]` / `[[deprecated("reason")]]` (C++14)
INFO(Deprecated)
/// `[[fallthrough]]` (C++17)
INFO(Fallthrough)
/// `[[maybe_unused]]` (C++17)
INFO(MaybeUnused)
/// `[[nodiscard]]` / `[[nodiscard("reason")]]` (C++17)
INFO(Nodiscard)
/// `[[likely]]` (C++20)
INFO(Likely)
/// `[[unlikely]]` (C++20)
INFO(Unlikely)
/// `[[no_unique_address]]` (C++20)
INFO(NoUniqueAddress)
/// `[[assume(expression)]]` (C++23)
INFO(Assume)
/// `[[indeterminate]]` (C++26)
INFO(Indeterminate)

#undef INFO
