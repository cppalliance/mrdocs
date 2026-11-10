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

#ifndef MRDOCS_API_HANDLEBARS_PLATFORM_HPP
#define MRDOCS_API_HANDLEBARS_PLATFORM_HPP

// Export/visibility macro for the handlebars library. Self-contained on purpose:
// handlebars does not depend on mrdocs-core's <mrdocs/Platform.hpp>. The expansion is
// driven by `MRDOCS_HANDLEBARS_SOURCE`, which is defined only while the
// handlebars library is built from source, and by `MRDOCS_DYNAMIC_LINK`, which
// marks code that links MrDocs dynamically, to the shared mrdocs-core or to a
// host executable (the `mrdocs-core` and `mrdocs::plugin-api` CMake targets
// define it for such code).
//
// On GCC and Clang the marker is always default visibility, static or shared:
// the library, mrdocs-core and the tool are compiled with hidden visibility,
// so an empty marker would hide these types in the units that use them. What
// carries the marker is exported and the rest stays local. On MSVC
// `MRDOCS_HANDLEBARS_SOURCE` comes first (dllexport, also in a static build),
// then `MRDOCS_DYNAMIC_LINK` (dllimport), else empty. See
// docs/modules/ROOT/pages/contribute/invariants.adoc.
//
// `MRDOCS_HANDLEBARS_VISIBLE` goes on a public class that has no
// `MRDOCS_HANDLEBARS_DECL` of its own. It is empty on MSVC and gives the class
// default visibility on GCC and Clang, so a member that lacks its own
// `MRDOCS_HANDLEBARS_DECL` still links there and fails only on MSVC. MrDocs
// records it as `type_visibility` when it parses.

// MrDocs defines `__MRDOCS__` when it parses. The marker then expands to the
// GNU attribute on every platform, since a parse on Windows uses the MSVC
// target and would otherwise record no marker.
#if defined(__MRDOCS__)
# define MRDOCS_HANDLEBARS_DECL __attribute__((__visibility__("default")))
# define MRDOCS_HANDLEBARS_VISIBLE __attribute__((__type_visibility__("default")))
#elif defined(_MSC_VER)
# if defined(MRDOCS_HANDLEBARS_SOURCE) // building the library from source
#  define MRDOCS_HANDLEBARS_DECL __declspec(dllexport)
# elif defined(MRDOCS_DYNAMIC_LINK)
#  define MRDOCS_HANDLEBARS_DECL __declspec(dllimport)
# else
#  define MRDOCS_HANDLEBARS_DECL
# endif
# define MRDOCS_HANDLEBARS_VISIBLE
#elif defined(__GNUC__)
# define MRDOCS_HANDLEBARS_DECL __attribute__((__visibility__("default")))
# define MRDOCS_HANDLEBARS_VISIBLE __attribute__((__visibility__("default")))
#else
# define MRDOCS_HANDLEBARS_DECL
# define MRDOCS_HANDLEBARS_VISIBLE
#endif

#endif
