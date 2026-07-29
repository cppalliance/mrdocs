//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Temp.hpp"
#include <mrdocs/Support/Report.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <cstdlib>


namespace mrdocs {

ScopedTempFile::
~ScopedTempFile()
{
    if (ok_)
    {
        llvm::sys::fs::remove(path_);
    }
}

ScopedTempFile::
ScopedTempFile(
    llvm::StringRef prefix,
    llvm::StringRef ext)
{
    llvm::SmallString<128> tempPath;
    ok_ = !llvm::sys::fs::createTemporaryFile(prefix, ext, tempPath);
    if (ok_)
    {
        path_ = tempPath;
    }
}

ScopedTempDirectory::
~ScopedTempDirectory() {
    if (*this && !path_.empty())
    {
        llvm::sys::fs::remove_directories(path_);
    }
}

ScopedTempDirectory::
ScopedTempDirectory(ScopedTempDirectory&& other) noexcept
    : path_(std::move(other.path_))
    , status_(other.status_)
{
    other.path_.clear();
    other.status_ = ErrorStatus::CannotCreateDirectories;
}

ScopedTempDirectory&
ScopedTempDirectory::
operator=(ScopedTempDirectory&& other) noexcept
{
    if (this != &other)
    {
        if (*this && !path_.empty())
        {
            llvm::sys::fs::remove_directories(path_);
        }
        path_ = std::move(other.path_);
        status_ = other.status_;
        other.path_.clear();
        other.status_ = ErrorStatus::CannotCreateDirectories;
    }
    return *this;
}

ScopedTempDirectory::
ScopedTempDirectory(
    llvm::StringRef prefix)
{
    llvm::SmallString<128> tempPath;
    if (llvm::sys::fs::createUniqueDirectory(prefix, tempPath))
    {
        status_ = ErrorStatus::CannotCreateDirectories;
        return;
    }
    path_ = tempPath;
}

ScopedTempDirectory::
ScopedTempDirectory(
    llvm::StringRef root,
    llvm::StringRef dir)
{
    llvm::SmallString<128> tempPath(root);
    llvm::sys::path::append(tempPath, dir);
    bool const exists = llvm::sys::fs::exists(tempPath);
    if (exists &&
        llvm::sys::fs::remove_directories(tempPath))
    {
        status_ = ErrorStatus::CannotDeleteExisting;
        return;
    }
    if (llvm::sys::fs::create_directories(tempPath))
    {
        status_ = ErrorStatus::CannotCreateDirectories;
        return;
    }
    path_ = tempPath;
}

Error
ScopedTempDirectory::
error() const
{
    if (status_ == ErrorStatus::CannotDeleteExisting)
    {
        return Error("Failed to delete existing directory");
    }
    if (status_ == ErrorStatus::CannotCreateDirectories)
    {
        return Error("Failed to create directories");
    }
    return Error();
}

Expected<ScopedTempDirectory>
makeScratchDirectory(
    std::string_view subject,
    std::vector<std::string> const& fallbacks)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // Build the ordered list of base directories to try.
    std::vector<std::string> candidates;
    if (char const* env = std::getenv("MRDOCS_TEMP_DIR"); env && *env)
    {
        candidates.emplace_back(env);
    }
    if (llvm::SmallString<128> cache; path::cache_directory(cache))
    {
        path::append(cache, "mrdocs");
        candidates.emplace_back(cache.str());
    }
    if (llvm::SmallString<128> sys;
        (path::system_temp_directory(true, sys), !sys.empty()))
    {
        candidates.emplace_back(sys.str());
    }
    candidates.insert(candidates.end(), fallbacks.begin(), fallbacks.end());

    // A directory-name component that is identifiable and made unique by
    // ScopedTempDirectory.
    std::string const prefix =
        "mrdocs-" + std::string(subject.empty() ? "scratch" : subject);

    for (std::string const& base : candidates)
    {
        // The base must exist (create it if missing) and be writable.
        if (base.empty() || fs::create_directories(base) || !fs::can_write(base))
        {
            continue;
        }
        llvm::SmallString<128> model(base);
        path::append(model, prefix);
        ScopedTempDirectory dir(model.str());
        if (dir)
        {
            return dir;
        }
    }
    return Unexpected(formatError(
        "could not create a writable scratch directory for \"{}\"", subject));
}

} // mrdocs
