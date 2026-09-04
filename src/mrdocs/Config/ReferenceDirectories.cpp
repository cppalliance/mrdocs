//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <cstdlib>
#include <string>
#include <utility>

// The library-relative step (below) is only meaningful for a shared mrdocs-core,
// which is installed as its own file next to the executable's directory. For the
// static library there is no such file -- the code is fused into the consuming
// executable -- so it is excluded and only the executable location is used.
#if defined(MRDOCS_SHARED_LINK)
# if defined(_WIN32)
#  include <llvm/Support/ConvertUTF.h>
#  include <windows.h>
# else
#  include <dlfcn.h>
# endif
#endif

namespace mrdocs {

namespace {

// Anchor whose address locates the module that provides mrdocs-core.
void mrdocsExecutableAnchor() {}

#if defined(MRDOCS_SHARED_LINK)
// Path of the shared mrdocs-core library (the module that contains the anchor),
// or empty if it cannot be determined.
std::string
mrdocsLibraryPath()
{
# if defined(_WIN32)
    HMODULE hmod = nullptr;
    if (!::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&mrdocsExecutableAnchor),
            &hmod))
    {
        return {};
    }
    std::wstring buf(MAX_PATH, L'\0');
    for (;;)
    {
        DWORD const n = ::GetModuleFileNameW(
            hmod, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0)
        {
            return {};
        }
        if (n < buf.size())
        {
            buf.resize(n);
            break;
        }
        if (buf.size() > 65536)
        {
            return {};
        }
        buf.resize(buf.size() * 2);
    }
    std::string utf8;
    if (llvm::convertWideToUTF8(buf, utf8))
    {
        return utf8;
    }
    return {};
# else
    Dl_info info{};
    if (::dladdr(reinterpret_cast<void*>(&mrdocsExecutableAnchor), &info)
        && info.dli_fname)
    {
        return info.dli_fname;
    }
    return {};
# endif
}
#endif // MRDOCS_SHARED_LINK

} // (anon)

ReferenceDirectories::
ReferenceDirectories(std::string root)
    : mrdocsRoot(std::move(root))
{
    llvm::SmallVector<char, 256> buf;
    if (!llvm::sys::fs::current_path(buf))
    {
        cwd.assign(buf.data(), buf.size());
    }

    // Precedence for the MrDocs root: MRDOCS_ROOT in the environment, then the
    // compile-time default passed as `root` (empty in MrDocs's own build; the
    // installed prefix for a downstream project via find_package), then a
    // location two levels below the file that provides mrdocs-core -- the shared
    // library when built shared (so a shared consumer resolves MrDocs's own
    // install), otherwise the running executable.
    if (char const* env = std::getenv("MRDOCS_ROOT"); env && *env)
    {
        mrdocsRoot = env;
    }
    else if (mrdocsRoot.empty())
    {
        std::string self;
#if defined(MRDOCS_SHARED_LINK)
        self = mrdocsLibraryPath();
#endif
        if (self.empty())
        {
            self = llvm::sys::fs::getMainExecutable(
                nullptr, reinterpret_cast<void*>(&mrdocsExecutableAnchor));
        }
        if (!self.empty())
        {
            mrdocsRoot = std::string(files::getParentDir(self, 2));
        }
    }
}

} // mrdocs
