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

// Export/visibility macro for the dom library. Self-contained on purpose:
// dom does not depend on mrdocs-core's <mrdocs/Platform.hpp>. The expansion is
// driven by `MRDOCS_DOM_SOURCE`, which is defined only while the
// dom library is built from source, and by `MRDOCS_DYNAMIC_LINK`, which
// marks code that links MrDocs dynamically, to the shared mrdocs-core or to a
// host executable (the `mrdocs-core` and `mrdocs::plugin-api` CMake targets
// define it for such code).
//
// On GCC and Clang the marker is always default visibility, static or shared:
// the library, mrdocs-core and the tool are compiled with hidden visibility,
// so an empty marker would hide these types in the units that use them. What
// carries the marker is exported and the rest stays local. On MSVC
// `MRDOCS_DOM_SOURCE` comes first (dllexport, also in a static build),
// then `MRDOCS_DYNAMIC_LINK` (dllimport), else empty. See
// docs/modules/ROOT/pages/contribute/invariants.adoc.
//
// `MRDOCS_DOM_VISIBLE` goes on a public class that has no `MRDOCS_DOM_DECL`
// of its own. It is empty on MSVC and gives the class default visibility on
// GCC and Clang, so a member that lacks its own `MRDOCS_DOM_DECL` still links
// there and fails only on MSVC. MrDocs records it as `type_visibility` when it
// parses.

// MrDocs defines `__MRDOCS__` when it parses. The marker then expands to the
// GNU attribute on every platform, since a parse on Windows uses the MSVC
// target and would otherwise record no marker.
#if defined(__MRDOCS__)
# define MRDOCS_DOM_DECL __attribute__((__visibility__("default")))
# define MRDOCS_DOM_VISIBLE __attribute__((__type_visibility__("default")))
#elif defined(_MSC_VER)
# if defined(MRDOCS_DOM_SOURCE) // building the library from source
#  define MRDOCS_DOM_DECL __declspec(dllexport)
# elif defined(MRDOCS_DYNAMIC_LINK)
#  define MRDOCS_DOM_DECL __declspec(dllimport)
# else
#  define MRDOCS_DOM_DECL
# endif
# define MRDOCS_DOM_VISIBLE
#elif defined(__GNUC__)
# define MRDOCS_DOM_DECL __attribute__((__visibility__("default")))
# define MRDOCS_DOM_VISIBLE __attribute__((__visibility__("default")))
#else
# define MRDOCS_DOM_DECL
# define MRDOCS_DOM_VISIBLE
#endif

#if __has_cpp_attribute(no_unique_address)
# define MRDOCS_DOM_NO_UNIQUE_ADDRESS [[no_unique_address]]
#elif __has_cpp_attribute(msvc::no_unique_address)
# define MRDOCS_DOM_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
# define MRDOCS_DOM_NO_UNIQUE_ADDRESS
#endif

#endif
