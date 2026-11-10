//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The attributes that mark the public API. They live in their own header
// because the lowest-level headers, which mrdocs/Platform.hpp itself pulls in,
// need them too.
//
// There is one export rule, on every platform: a declaration marked
// `MRDOCS_DECL` is exported from the library and from a static host that links
// it, and everything else stays local. The libraries and the hosts are
// compiled with hidden visibility. While MrDocs is built from source, the
// marker expands to the attribute that overrides it (`dllexport` on MSVC,
// default visibility on GCC and Clang). A class marked as a whole also exports
// what the compiler derives from it: its vtable, type information and thunks.
// See docs/modules/ROOT/pages/contribute/invariants/binary-compatibility.adoc.

#ifndef MRDOCS_API_SUPPORT_EXPORT_HPP
#define MRDOCS_API_SUPPORT_EXPORT_HPP

//------------------------------------------------
//
// Shared Libraries
//
//------------------------------------------------

// `MRDOCS_SOURCE` is defined when MrDocs is built from source, and it comes
// first: the API is marked for export even in a static build, where a plugin
// resolves it against the host.

// `MRDOCS_DYNAMIC_LINK` marks code that links MrDocs dynamically, either to
// the shared library or to a host executable. The API is then imported from
// that module. The `mrdocs-core` and `mrdocs::plugin-api` CMake targets define
// it for such code. Code that links MrDocs directly defines nothing, and the
// API is neither imported nor exported.

// MrDocs defines `__MRDOCS__` when it parses, and the markers then expand the
// same way on every platform. A parse on Windows uses the MSVC target, where
// `MRDOCS_VISIBLE` is empty and no marker would be recorded.
// `type_visibility` covers only the type information, which is what
// `MRDOCS_VISIBLE` is for, and it is recorded under its own attribute name.

#if defined(__MRDOCS__)
#define MRDOCS_DECL __attribute__((__visibility__("default")))
#define MRDOCS_VISIBLE __attribute__((__type_visibility__("default")))
#elif defined(_MSC_VER)
#if defined(MRDOCS_SOURCE) // building MrDocs from source
#define MRDOCS_DECL __declspec(dllexport)
#elif defined(MRDOCS_DYNAMIC_LINK)
#define MRDOCS_DECL __declspec(dllimport)
#else
#define MRDOCS_DECL
#endif
#define MRDOCS_VISIBLE
#elif defined(__GNUC__)
#if defined(MRDOCS_SOURCE) // building MrDocs from source
#define MRDOCS_DECL __attribute__((__visibility__("default")))
#define MRDOCS_VISIBLE __attribute__((__visibility__("default")))
#elif defined(MRDOCS_DYNAMIC_LINK)
#define MRDOCS_DECL __attribute__((__visibility__("default")))
#define MRDOCS_VISIBLE __attribute__((__visibility__("default")))
#else
#define MRDOCS_DECL
#define MRDOCS_VISIBLE
#endif
#else
#error unknown platform for dynamic linking
#endif

#endif // MRDOCS_API_SUPPORT_EXPORT_HPP
