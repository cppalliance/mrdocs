//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_MRDOCSFILESYSTEM_HPP
#define MRDOCS_LIB_AST_MRDOCSFILESYSTEM_HPP

#include <lib/ConfigImpl.hpp>
#include <clang/Lex/HeaderSearch.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace mrdocs {

// A proxy and overlay FS that, when a file is missing, might serve an
// adjusted or empty file from an in-memory FS and remembers it, so repeated
// opens are cheap.
// This is used to work around missing headers in some environments and
// avoid hard failures that wouldn't allow documentation generation
// unless all dependencies were present.
// We have a config option where the user can specify the glob patterns
// for include files that should be treated this way. The name
// of the option is `forgive-missing-includes`. The user can specify
// the "**" pattern to forgive all missing includes or specific
// patterns like "llvm/**" to forgive all includes from LLVM.
class MrDocsFileSystem : public llvm::vfs::FileSystem {
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> Real;
    std::shared_ptr<llvm::vfs::InMemoryFileSystem> Mem;
    ConfigImpl const &config_;
    clang::HeaderSearch* HeaderSearch = nullptr;

    mutable std::mutex MemMu;
    std::atomic<bool> CWDSet{ false };
    std::string CWD;

    // --- policy helpers ---
    bool
    matchesPrefixSet(llvm::StringRef Path) const
    {
        auto matchesPrefixSetImpl =
            [](llvm::StringRef Path, auto const &Prefixes) {
            for (auto const &P: Prefixes)
            {
                auto pos = Path.find_insensitive(P);
                MRDOCS_CHECK_OR_CONTINUE(pos != llvm::StringRef::npos);
                if (pos == 0 || Path[pos - 1] == '/' || Path[pos - 1] == '\\')
                {
                    return true;
                }
            }
            return false;
        };
        if (matchesPrefixSetImpl(Path, config_->missingIncludePrefixes))
        {
            return true;
        }
        auto shimParents = std::views::keys(config_->missingIncludeShims)
                           | std::views::transform([](std::string const &s) {
            return llvm::sys::path::parent_path(s);
        });
        return matchesPrefixSetImpl(Path, shimParents);
    }

    Optional<std::pair<std::string_view, std::string_view>>
    matchShim(llvm::StringRef Path) const
    {
        for (auto const &[K, P]: config_->missingIncludeShims)
        {
            if (Path.ends_with_insensitive(K))
            {
                return std::make_pair(std::string_view(K), std::string_view(P));
            }
        }
        return std::nullopt;
    }

    std::string
    wrapShim(std::string_view path, std::string_view contents) const
    {
        std::string shim_macro = "MRDOCS_DYNAMIC_CONFIG_FILE_SHIM_";
        shim_macro.reserve(shim_macro.size() + path.size());
        for (char c: path)
        {
            if (!std::isalnum(c))
            {
                shim_macro += '_';
            } else
            {
                shim_macro += std::toupper(c);
            }
        }
        std::string result;
        result += "#ifndef ";
        result += shim_macro;
        result += "\n#define ";
        result += shim_macro;
        result += "\n";
        result += contents;
        result += "\n#endif // ";
        result += shim_macro;
        result += "\n";
        return result;
    }

    bool
    looksLikeDirectory(llvm::StringRef P)
    {
        // If it's dirsy, treat as directory.
        if (P.ends_with("/") || P.ends_with("\\"))
        {
            return true;
        }
        // If it has an extension, treat as file
        llvm::StringRef Base = llvm::sys::path::filename(P);
        if (llvm::sys::path::has_extension(Base))
        {
            return false;
        }
        // If there's no extension, it's ambiguous, but there are still
        // a few cases where we can be sure it's a file.
        for (auto &[key, value]: config_->missingIncludeShims)
        {
            if (P.ends_with("/" + key) || P.ends_with("\\" + key))
            {
                return false;
            }
        }
        return true;
    }


    // Ensure Mem has a file at Path with given contents (thread-safe).
    void
    ensureMemFile(llvm::StringRef Path, llvm::StringRef Contents)
    {
        std::lock_guard<std::mutex> lock(MemMu);
        if (!Mem->exists(Path))
        {
            auto Buf = llvm::MemoryBuffer::getMemBufferCopy(Contents, Path);
            Mem->addFile(Path, /*ModTime*/ 0, std::move(Buf));
        }
    }

    // Make a synthetic directory status (no need to store it in MemFS).
    static llvm::vfs::Status
    makeDirStatus(llvm::StringRef Path)
    {
        using namespace llvm::sys::fs;
        // Any UniqueID is fine for synthetic entries.
        static std::atomic<uint64_t> NextInode{ 1 };
        auto Now = std::chrono::time_point_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now());
        llvm::sys::TimePoint<> MTime(Now.time_since_epoch());
        return {
            Path, // Name (Twine-constructible)
            UniqueID(/*Dev*/ 0, /*Ino*/ NextInode++),
            MTime, // <-- third arg is MTime
            /*User*/ 0,
            /*Group*/ 0,
            /*Size*/ 0,
            file_type::directory_file,
            perms::all_all
        };
    }

    // Check if the filesystem supports virtual files because of
    // configuration options
    bool
    containsVirtualFiles() const
    {
        return !config_->missingIncludePrefixes.empty()
               || !config_->missingIncludeShims.empty();
    }

public:
    MrDocsFileSystem(
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> RealFS,
        ConfigImpl const &Cfg)
        : Real(std::move(RealFS))
        , Mem(std::make_shared<llvm::vfs::InMemoryFileSystem>())
        , config_(Cfg)
    {}

    // --- FileSystem interface ---

    llvm::ErrorOr<llvm::vfs::Status>
    status(llvm::Twine const &Path) override
    {
        llvm::ErrorOr<llvm::vfs::Status> RS = Real->status(Path);
        if (RS || !containsVirtualFiles())
        {
            return RS;
        }

        auto P = Path.str();
        if (auto MS = Mem->status(P))
        {
            return MS;
        }

        std::string relPath = P;
        if (matchesPrefixSet(P))
        {
            if (looksLikeDirectory(P))
            {
                return makeDirStatus(P);
            }

            ensureMemFile(P, "");
            if (auto MS = Mem->status(P))
            {
                return MS;
            }
            // else fall through to propagate original status
        }

        // propagate original real error (typically ENOENT)
        return RS;
    }

    llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>>
    openFileForRead(llvm::Twine const &Path) override
    {
        auto RF = Real->openFileForRead(Path);
        if (RF || !containsVirtualFiles())
        {
            return RF;
        }

        auto P = Path.str();
        if (auto MF = Mem->openFileForRead(P))
        {
            return MF;
        }

        if (matchesPrefixSet(P))
        {
            auto shim = matchShim(P);
            if (shim)
            {
                ensureMemFile(P, wrapShim(shim->first, shim->second));
            } else
            {
                ensureMemFile(P, "");
            }

            if (auto MF = Mem->openFileForRead(P))
            {
                return MF;
            }
            // else fall through to propagate original error
        }

        return RF; // return same error as first attempt
    }

    // In MrDocsFileSystem class (override of vfs::FileSystem)
    llvm::vfs::directory_iterator
    dir_begin(llvm::Twine const &Dir, std::error_code &EC) override
    {
        // Try the real filesystem first.
        auto It = Real->dir_begin(Dir, EC);
        if (!EC)
        {
            return It;
        }

        // If real FS failed, try the in-memory (shim/stub) FS.
        EC = std::error_code();
        auto ItMem = Mem->dir_begin(Dir, EC);
        if (!EC)
        {
            return ItMem;
        }

        // Both failed: return an empty (end) iterator and no error.
        EC = std::error_code();
        return llvm::vfs::directory_iterator();
    }

    std::error_code
    setCurrentWorkingDirectory(llvm::Twine const &Path) override
    {
        auto S = Path.str();
        if (auto EC = Real->setCurrentWorkingDirectory(S))
        {
            return EC;
        }
        CWD = std::move(S);
        CWDSet = true;
        return {};
    }

    llvm::ErrorOr<std::string>
    getCurrentWorkingDirectory() const override
    {
        if (CWDSet)
        {
            return CWD;
        }
        return Real->getCurrentWorkingDirectory();
    }

    std::error_code
    getRealPath(llvm::Twine const &Path, llvm::SmallVectorImpl<char> &Output)
        override
    {
        // Mem paths are synthetic; passthrough is fine.
        return Real->getRealPath(Path, Output);
    }

    bool
    addVirtualFile(llvm::StringRef path, llvm::StringRef contents)
    {
        std::lock_guard<std::mutex> lock(MemMu);
        auto Buf = llvm::MemoryBuffer::getMemBufferCopy(contents, path);
        return Mem->addFile(path, /*MTime*/ 0, std::move(Buf));
    }
};

inline llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>
createMrDocsFileSystem(ConfigImpl const &Cfg)
{
    auto Real = llvm::vfs::getRealFileSystem();
    return { new MrDocsFileSystem(Real, Cfg) };
}

} // namespace mrdocs

#endif // MRDOCS_LIB_AST_MRDOCSFILESYSTEM_HPP
