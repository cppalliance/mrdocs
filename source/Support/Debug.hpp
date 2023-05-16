//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
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

#if __has_include(<format>)
    #define MRDOX_HAS_CXX20_FORMAT

    #include <format>
    #include <string>
    #include <mrdox/MetadataFwd.hpp>
#endif

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

#ifdef MRDOX_HAS_CXX20_FORMAT
    template<>
    struct std::formatter<clang::mrdox::SymbolID>
        : std::formatter<std::string>
    {
        std::format_context::iterator format(
            const clang::mrdox::SymbolID& s, 
            std::format_context& ctx) const;
    };

    template<>
    struct std::formatter<clang::mrdox::OptionalSymbolID>
        : std::formatter<clang::mrdox::SymbolID>
    {
        std::format_context::iterator format(
            const clang::mrdox::OptionalSymbolID& s, 
            std::format_context& ctx) const;
    };

    template<>
    struct std::formatter<clang::mrdox::InfoType>
        : std::formatter<std::string>
    {
        std::format_context::iterator format(
            clang::mrdox::InfoType t, 
            std::format_context& ctx) const;
    };

    template<>
    struct std::formatter<clang::mrdox::Access> 
        : std::formatter<std::string>
    {
        std::format_context::iterator format(
            clang::mrdox::Access a, 
            std::format_context& ctx) const;
    };

    template<>
    struct std::formatter<clang::mrdox::Reference> 
        : std::formatter<std::string>
    {
        std::format_context::iterator format(
            const clang::mrdox::Reference& r, 
            std::format_context& ctx) const;
    };

    template<>
    struct std::formatter<clang::mrdox::RefWithAccess> 
        : std::formatter<std::string>
    {
        std::format_context::iterator format(
            const clang::mrdox::RefWithAccess& r, 
            std::format_context& ctx) const;
    };

    template<>
    struct std::formatter<clang::mrdox::Info> 
        : std::formatter<std::string>
    {
        std::format_context::iterator format(
            const clang::mrdox::Info& i, 
            std::format_context& ctx) const;
    };

    #define PRINT_FMT(fmt, ...) (debug_outs() << std::format(fmt \
        __VA_OPT__(,) __VA_ARGS__))
    #define PRINT_FUNC_AND_NAME(name) PRINT_FMT("{:<48} {}\n", \
        (std::string(std::source_location::current().function_name()) + ":"), name)
#else
    #define PRINT_FMT(fmt, ...)
    #define PRINT_FUNC_AND_NAME(name)
#endif

#endif
