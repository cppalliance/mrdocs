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

// Export/visibility macro for the handlebars library. Self-contained on
// purpose: handlebars depends neither on mrdocs-core's <mrdocs/Platform.hpp>
// nor on the defines mrdocs-core and the tool are built with. The expansion is
// driven by `MRDOCS_HANDLEBARS_BUILDING`, which only the handlebars target
// defines, and by the `MRDOCS_STATIC_LINK` define the build passes to every
// target.
//
// `MRDOCS_HANDLEBARS_BUILDING` comes first: a static build exports the API too.

#if defined(_MSC_VER)
# if defined(MRDOCS_HANDLEBARS_BUILDING) // building the library
#  define MRDOCS_HANDLEBARS_DECL __declspec(dllexport)
# elif defined(MRDOCS_STATIC_LINK)
#  define MRDOCS_HANDLEBARS_DECL
# else
#  define MRDOCS_HANDLEBARS_DECL __declspec(dllimport)
# endif
#elif defined(__GNUC__)
# if defined(MRDOCS_HANDLEBARS_BUILDING) || defined(MRDOCS_STATIC_LINK)
#  define MRDOCS_HANDLEBARS_DECL
# else
#  define MRDOCS_HANDLEBARS_DECL __attribute__((__visibility__("default")))
# endif
#else
# define MRDOCS_HANDLEBARS_DECL
#endif

#endif
