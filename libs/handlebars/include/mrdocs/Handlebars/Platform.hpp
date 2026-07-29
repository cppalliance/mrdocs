//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_PLATFORM_HPP
#define MRDOCS_API_HANDLEBARS_PLATFORM_HPP

// Export/visibility macro for the handlebars library. Self-contained on purpose:
// handlebars does not depend on mrdocs-core's <mrdocs/Platform.hpp>. The
// expansion is driven by the same MRDOCS_STATIC_LINK / MRDOCS_SHARED_LINK define
// the build passes to every target; with the default static build it is empty.

#if defined(MRDOCS_STATIC_LINK)
# define MRDOCS_HANDLEBARS_DECL
#elif defined(_MSC_VER)
# if defined(MRDOCS_TOOL)
#  define MRDOCS_HANDLEBARS_DECL __declspec(dllexport)
# else
#  define MRDOCS_HANDLEBARS_DECL __declspec(dllimport)
# endif
#elif defined(__GNUC__)
# if defined(MRDOCS_TOOL)
#  define MRDOCS_HANDLEBARS_DECL
# else
#  define MRDOCS_HANDLEBARS_DECL __attribute__((__visibility__("default")))
# endif
#else
# define MRDOCS_HANDLEBARS_DECL
#endif

#endif
