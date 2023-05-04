//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_DEBUG_HPP
#define MRDOX_DEBUG_HPP

#include <mrdox/Platform.hpp>
#if ! defined(NDEBUG)
#include <llvm/Support/ErrorHandling.h>
#endif
#include <llvm/Support/raw_ostream.h>

// Some nice odds and ends such as leak checking
// and redirection to the Visual Studio output window.

namespace clang {
namespace mrdox {

/** Return a stream which writes output to the debugger.
*/
/** @{ */
MRDOX_DECL llvm::raw_ostream& debug_outs();
MRDOX_DECL llvm::raw_ostream& debug_errs();
/** @} */

/** Enable debug heap checking.
*/
MRDOX_DECL void debugEnableHeapChecking();

#if defined(NDEBUG)
#define Assert(expr) ((void)0)
#else
#define Assert(expr) ((!!(expr))? ((void)0): llvm_unreachable(#expr))
#endif

#define static_error(msg, value) \
    static_assert(!std::is_same_v<decltype(value),decltype(value)>,msg)

} // mrdox
} // clang

#endif
