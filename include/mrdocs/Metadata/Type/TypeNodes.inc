//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef INFO
#define INFO(PascalName)
#endif

/// A Named type
INFO(Named)
/// A decltype type
INFO(Decltype)
/// An auto type
INFO(Auto)
/// An LValueReference type
INFO(LValueReference)
/// An RValueReference type
INFO(RValueReference)
/// A Pointer type
INFO(Pointer)
/// A MemberPointer type
INFO(MemberPointer)
/// An Array type
INFO(Array)
/// A Function type
INFO(Function)

#undef INFO

