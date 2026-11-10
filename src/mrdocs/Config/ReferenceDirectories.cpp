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
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>

#if defined(_WIN32)
# include <llvm/Support/ConvertUTF.h>
# include <windows.h>
#else
# include <dlfcn.h>
# if defined(__APPLE__)
#  include <mach-o/dyld.h>
# elif defined(__linux__)
#  include <link.h>
# endif
#endif

namespace mrdocs {

namespace {

// Anchor whose address locates the module that provides mrdocs-core.
void mrdocsExecutableAnchor() {}

#if defined(__linux__)
// Whether the anchor is in the first object `dl_iterate_phdr` reports, which
// is the main program.
bool
anchorIsInMainProgram()
{
    struct State
    {
        ElfW(Addr) anchor;
        bool found = false;
    } state{reinterpret_cast<ElfW(Addr)>(&mrdocsExecutableAnchor)};
    ::dl_iterate_phdr(
        [](dl_phdr_info* info, std::size_t, void* data) -> int
        {
            auto& st = *static_cast<State*>(data);
            for (ElfW(Half) i = 0; i < info->dlpi_phnum; ++i)
            {
                ElfW(Phdr) const& ph = info->dlpi_phdr[i];
                if (ph.p_type != PT_LOAD)
                {
                    continue;
                }
                ElfW(Addr) const begin = info->dlpi_addr + ph.p_vaddr;
                if (st.anchor >= begin && st.anchor < begin + ph.p_memsz)
                {
                    st.found = true;
                    break;
                }
            }
            // Only the first object is the main program.
            return 1;
        },
        &state);
    return state.found;
}
#endif

// Path of the module that contains the anchor: the shared mrdocs-core library,
// or the executable when the core is linked into it. Empty if it cannot be
// determined. `isMain` tells whether that module is the main program.
std::string
mrdocsModulePath(bool& isMain)
{
    isMain = false;
#if defined(_WIN32)
    HMODULE hmod = nullptr;
    if (!::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&mrdocsExecutableAnchor),
            &hmod))
    {
        return {};
    }
    isMain = hmod == ::GetModuleHandleW(nullptr);
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
#else
    Dl_info info{};
    if (!::dladdr(reinterpret_cast<void*>(&mrdocsExecutableAnchor), &info)
        || !info.dli_fname)
    {
        return {};
    }
# if defined(__APPLE__)
    isMain = info.dli_fbase == _dyld_get_image_header(0);
# elif defined(__linux__)
    isMain = anchorIsInMainProgram();
# endif
    return info.dli_fname;
#endif
}

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
    // location two levels below the module that holds mrdocs-core (the
    // executable when the core is linked into it).
    if (char const* env = std::getenv("MRDOCS_ROOT"); env && *env)
    {
        mrdocsRoot = env;
    }
    else if (mrdocsRoot.empty())
    {
        bool isMain = false;
        std::string self = mrdocsModulePath(isMain);
        if (self.empty() || isMain)
        {
            // The loader can report the main program by the relative path it
            // was started with. getMainExecutable always gives an absolute one.
            self = llvm::sys::fs::getMainExecutable(
                nullptr, reinterpret_cast<void*>(&mrdocsExecutableAnchor));
        }
        else
        {
            // A library found through a relative search path is reported by a
            // relative path too.
            llvm::SmallString<256> abs(self);
            if (!llvm::sys::fs::make_absolute(abs))
            {
                self.assign(abs.data(), abs.size());
            }
        }
        if (!self.empty())
        {
            mrdocsRoot = std::string(files::getParentDir(self, 2));
        }
    }
}

} // mrdocs
