//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_PLATFORM_HPP
#define MRDOCS_API_DOM_PLATFORM_HPP

// Export/visibility macros for the dom library. Self-contained on purpose: dom
// does not depend on mrdocs-core's <mrdocs/Platform.hpp>. The expansion is
// driven by the same MRDOCS_TOOL / MRDOCS_STATIC_LINK defines the build passes
// to every target.
//
// MRDOCS_TOOL, defined when MrDocs itself is being built, comes first: the API
// is marked for export even in a static build. On MSVC that marking is what puts
// the symbols a plugin calls in the tool's export table; elsewhere the macro
// expands to nothing either way, and the symbols are reachable because the tool
// is linked with its exports enabled. A static consumer of the library, which
// defines MRDOCS_STATIC_LINK and not MRDOCS_TOOL, still sees plain declarations.

#if defined(_MSC_VER)
# if defined(MRDOCS_TOOL) // building MrDocs
#  define MRDOCS_DOM_DECL __declspec(dllexport)
# elif defined(MRDOCS_STATIC_LINK)
#  define MRDOCS_DOM_DECL
# else
#  define MRDOCS_DOM_DECL __declspec(dllimport)
# endif
#elif defined(__GNUC__)
# if defined(MRDOCS_TOOL) || defined(MRDOCS_STATIC_LINK)
#  define MRDOCS_DOM_DECL
# else
#  define MRDOCS_DOM_DECL __attribute__((__visibility__("default")))
# endif
#else
# define MRDOCS_DOM_DECL
#endif

#if __has_cpp_attribute(no_unique_address)
# define MRDOCS_DOM_NO_UNIQUE_ADDRESS [[no_unique_address]]
#elif __has_cpp_attribute(msvc::no_unique_address)
# define MRDOCS_DOM_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
# define MRDOCS_DOM_NO_UNIQUE_ADDRESS
#endif

#endif
