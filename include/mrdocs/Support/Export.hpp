//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The attributes that carry the public API across a shared library
// boundary. They live in their own header because the lowest-level
// headers, which mrdocs/Platform.hpp itself pulls in, need them too.

#ifndef MRDOCS_API_SUPPORT_EXPORT_HPP
#define MRDOCS_API_SUPPORT_EXPORT_HPP

//------------------------------------------------
//
// Shared Libraries
//
//------------------------------------------------

// MRDOCS_TOOL is defined when MrDocs itself is being built, and it comes
// first: the API is marked for export even in a static build.

#if defined(_MSC_VER)
# define MRDOCS_SYMBOL_EXPORT __declspec(dllexport)
# define MRDOCS_SYMBOL_IMPORT __declspec(dllimport)
# if defined(MRDOCS_TOOL) // building MrDocs
#  define MRDOCS_DECL MRDOCS_SYMBOL_EXPORT
# elif defined(MRDOCS_STATIC_LINK)
#  define MRDOCS_DECL
# else
#  define MRDOCS_DECL MRDOCS_SYMBOL_IMPORT
# endif
# define MRDOCS_VISIBLE

#elif defined(__GNUC__)
# if defined(MRDOCS_TOOL) // building MrDocs
#  define MRDOCS_DECL
#  define MRDOCS_VISIBLE __attribute__((__visibility__("default")))
# elif defined(MRDOCS_STATIC_LINK)
#  define MRDOCS_DECL
#  define MRDOCS_VISIBLE
# else
#  define MRDOCS_DECL __attribute__((__visibility__("default")))
#  define MRDOCS_VISIBLE __attribute__((__visibility__("default")))
# endif
#else
# error unknown platform for dynamic linking
#endif

#endif // MRDOCS_API_SUPPORT_EXPORT_HPP
