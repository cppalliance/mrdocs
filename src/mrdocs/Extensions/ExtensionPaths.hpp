//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_EXTENSIONPATHS_HPP
#define MRDOCS_LIB_EXTENSIONS_EXTENSIONPATHS_HPP

#include <filesystem>
#include <string>
#include <string_view>

namespace mrdocs {

// Return `utf8` as a path that names the file the UTF-8 text names. Windows
// reads a path built from a std::string in the ANSI code page, and it looks
// for another file when the text has a character the code page lacks. The
// text is therefore handed over as a std::u8string, which every platform
// reads as UTF-8.
inline std::filesystem::path
utf8Path(std::string_view const utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

// Return `path` as UTF-8 text, the encoding the addon roots and the
// extensions keep every path in. On Windows, path::string() would give the
// code page of the system, and it throws for a character the code page
// lacks.
inline std::string
utf8String(std::filesystem::path const& path)
{
    std::u8string const text = path.u8string();
    return std::string(text.begin(), text.end());
}

} // mrdocs

#endif
