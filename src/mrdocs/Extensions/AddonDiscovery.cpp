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

#include "AddonDiscovery.hpp"
#include "ExtensionPaths.hpp"
#include <mrdocs/Support/AddonRoots.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <llvm/ADT/StringRef.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <utility>

namespace mrdocs {

namespace {

// Whether `fileName` ends with `extension`. Windows and macOS ignore case
// in file names by default, so Stats.DLL names a library there.
bool
endsWithExtension(std::string_view fileName, std::string_view extension)
{
#if defined(_WIN32) || defined(__APPLE__)
    return llvm::StringRef(fileName).ends_with_insensitive(extension);
#else
    return fileName.ends_with(extension);
#endif
}

// Whether a file name is that of a loadable library. CMake gives a
// module library the .so extension on macOS, so a native extension there
// can carry either name.
bool
isLibraryName(std::string_view fileName)
{
#ifdef _WIN32
    constexpr std::string_view extensions[] = {".dll"};
#elif defined(__APPLE__)
    constexpr std::string_view extensions[] = {".dylib", ".so"};
#else
    constexpr std::string_view extensions[] = {".so"};
#endif
    return std::ranges::any_of(
        extensions,
        [fileName](std::string_view extension)
        {
            return endsWithExtension(fileName, extension);
        });
}

// The kind of extension a file name is the entry of, if any.
bool
entryKind(std::string_view fileName, ExtensionKind& kind)
{
    if (fileName.ends_with(".lua"))
    {
        kind = ExtensionKind::Lua;
        return true;
    }
    if (fileName.ends_with(".js"))
    {
        kind = ExtensionKind::JavaScript;
        return true;
    }
    if (isLibraryName(fileName))
    {
        kind = ExtensionKind::Native;
        return true;
    }
    return false;
}

// Whether the filesystem entry is a regular file, a link to one, or a link
// whose target is missing (a link loop counts as missing), so that the run
// names it. A FIFO, a device, a directory, or a link to one of them is not
// handed to a script engine or to the dynamic loader, which could block on
// it. The scan uses std::filesystem rather than `forEachFile`, which skips
// symbolic links.
bool
isFileOrDanglingLink(std::filesystem::path const& path)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    return fs::is_regular_file(path, ec) ||
        (fs::is_symlink(path, ec) && !fs::exists(path, ec));
}

// Whether `text` starts with `prefix`, ignoring case where file names do.
bool
startsWithName(std::string_view text, std::string_view prefix)
{
#if defined(_WIN32) || defined(__APPLE__)
    return llvm::StringRef(text).starts_with_insensitive(prefix);
#else
    return text.starts_with(prefix);
#endif
}

// A name without the `lib` prefix of a native library.
std::string
withoutLibPrefix(std::string_view name)
{
    if (name.size() > 3 && startsWithName(name, "lib"))
    {
        name.remove_prefix(3);
    }
    return std::string(name);
}

// The name of the extension a file is the entry of: the file name without
// its suffix, and without the `lib` prefix of a native library.
std::string
fileStem(std::string_view fileName, ExtensionKind kind)
{
    std::string_view const stem = fileName.substr(0, fileName.rfind('.'));
    return kind == ExtensionKind::Native ?
        withoutLibPrefix(stem) : std::string(stem);
}

// The key two extension names are compared by: file names ignore case on
// Windows and macOS.
std::string
nameKey(std::string name)
{
#if defined(_WIN32) || defined(__APPLE__)
    std::ranges::transform(name, name.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
#endif
    return name;
}

// An entry, with the name of the extension it belongs to.
struct Candidate
{
    std::string name;
    ExtensionEntry entry;
};

// The names a directory `name` can hold its entry file under. The names of
// the libraries other platforms load are left out, so a directory can carry
// the libraries of several platforms side by side.
std::vector<std::string>
directoryCandidates(std::string const& name)
{
    std::vector<std::string> names = {
        name + ".lua",
        "init.lua",
        name + ".js",
        "index.js",
        name + ".so",
        "lib" + name + ".so",
        name + ".dylib",
        "lib" + name + ".dylib",
        name + ".dll"};
    std::erase_if(names, [](std::string const& candidate)
        {
            ExtensionKind kind = ExtensionKind::Lua;
            return !entryKind(candidate, kind); });
    return names;
}

// Resolve a directory `<name>/` to its extension: the entry file it holds.
// A directory with no entry file is skipped. When it holds both names of one
// language, `init.lua` wins over `<name>.lua` and `index.js` over
// `<name>.js`, and the file left out is named in a debug message. Two
// languages, or two library names, are an error.
Expected<void>
collectDirectory(
    std::filesystem::path const& dir,
    std::vector<Candidate>& found,
    report::Reporter& reporter)
{
    std::string const name = utf8String(dir.filename());
    std::vector<std::string> present;
    for (std::string const& candidate : directoryCandidates(name))
    {
        std::filesystem::path const path = dir / utf8Path(candidate);
        if (!isFileOrDanglingLink(path))
        {
            continue;
        }
        // Two candidate names can be one file: `<name>.lua` and `init.lua`
        // in a directory called `init`, or any pair on a filesystem that
        // ignores case.
        bool const repeated = std::ranges::any_of(
            present,
            [&](std::string const& other)
            {
                std::error_code ec;
                return std::filesystem::equivalent(
                    path, dir / utf8Path(other), ec);
            });
        if (!repeated)
        {
            present.push_back(candidate);
        }
    }
    std::string const dirText = files::makePosixStyle(utf8String(dir));
    for (auto const& [convention, byName] : {
             std::pair{std::string("init.lua"), name + ".lua"},
             std::pair{std::string("index.js"), name + ".js"}})
    {
        if (byName == convention)
        {
            continue;
        }
        auto const left = std::ranges::find(present, byName);
        if (left != present.end() &&
            std::ranges::find(present, convention) != present.end())
        {
            reporter.debug(
                "The extension directory \"{}\" holds \"{}\" and \"{}\", "
                "so \"{}\" is the entry file and \"{}\" is left out",
                dirText, convention, byName, convention, byName);
            present.erase(left);
        }
    }
    if (present.empty())
    {
        reporter.debug(
            "The extension directory \"{}\" has no entry file, "
            "so it is skipped",
            dirText);
        return {};
    }
    if (present.size() > 1)
    {
        std::string list;
        for (std::string const& candidate : present)
        {
            if (!list.empty())
            {
                list += ", ";
            }
            list += candidate;
        }
        return Unexpected(formatError(
            "the extension directory \"{}\" has more than one entry "
            "file: {}",
            dirText, list));
    }
    Candidate candidate;
    candidate.entry.directory = dirText;
    candidate.entry.path = files::makePosixStyle(
        utf8String(dir / utf8Path(present.front())));
    ExtensionKind kind = ExtensionKind::Lua;
    entryKind(present.front(), kind);
    candidate.entry.kind = kind;
    candidate.name = kind == ExtensionKind::Native ?
        withoutLibPrefix(name) : name;
    found.push_back(std::move(candidate));
    return {};
}

// Append the extensions found directly under `dir`, an extensions/
// directory. A directory that cannot be read is a warning rather than an
// error: it may hold nothing MrDocs wanted, and saying so beats leaving
// an extension the user installed out of the run without a word.
Expected<void>
collectFromDirectory(
    std::string const& dir,
    std::vector<Candidate>& found,
    report::Reporter& reporter)
{
    namespace fs = std::filesystem;
    std::vector<Candidate> local;
    std::error_code ec;
    fs::directory_iterator const end{};
    for (fs::directory_iterator it(utf8Path(dir), ec);
        !ec && it != end;
        it.increment(ec))
    {
        fs::path const& path = it->path();
        std::string const fileName = utf8String(path.filename());
        std::error_code typeEc;
        if (it->is_directory(typeEc))
        {
            MRDOCS_TRY(collectDirectory(path, local, reporter));
            continue;
        }
        ExtensionKind kind = ExtensionKind::Lua;
        if (entryKind(fileName, kind) && isFileOrDanglingLink(path))
        {
            Candidate candidate;
            candidate.name = fileStem(fileName, kind);
            candidate.entry.kind = kind;
            candidate.entry.path = files::makePosixStyle(utf8String(path));
            local.push_back(std::move(candidate));
        }
    }
    if (ec)
    {
        reporter.warn(
            "The extensions directory \"{}\" could not be read: {}",
            dir, ec.message());
    }
    // Two entries of one directory with the same name are an error, so a
    // script and a library cannot both claim an extension.
    std::ranges::sort(local,
        [](Candidate const& a, Candidate const& b)
        {
            std::string const keyA = nameKey(a.name);
            std::string const keyB = nameKey(b.name);
            return keyA != keyB ? keyA < keyB : a.entry.path < b.entry.path;
        });
    for (std::size_t i = 1; i < local.size(); ++i)
    {
        std::string const key = nameKey(local[i].name);
        if (nameKey(local[i - 1].name) != key)
        {
            continue;
        }
        std::string list;
        for (Candidate const& other : local)
        {
            if (nameKey(other.name) != key)
            {
                continue;
            }
            if (!list.empty())
            {
                list += ", ";
            }
            if (other.entry.directory.empty())
            {
                list += utf8String(utf8Path(other.entry.path).filename());
            }
            else
            {
                list += utf8String(utf8Path(other.entry.directory).filename());
                list += '/';
            }
        }
        return Unexpected(formatError(
            "{}: more than one extension is named \"{}\" ({})",
            dir, local[i].name, list));
    }
    for (Candidate& candidate : local)
    {
        found.push_back(std::move(candidate));
    }
    return {};
}

// Remove from `found` each native entry that names the same file as an
// earlier one. One library can be reached under two names, for example
// through a link with another stem in another addon root, and running its
// entry point twice would fail on the ids it installed the first time.
// Identity comes from the filesystem, so a link is still recognized.
void
removeRepeatedLibraries(std::vector<ExtensionEntry>& found)
{
    std::vector<ExtensionEntry> kept;
    kept.reserve(found.size());
    for (ExtensionEntry& entry : found)
    {
        bool known = false;
        if (entry.kind == ExtensionKind::Native)
        {
            known = std::ranges::any_of(
                kept,
                [&entry](ExtensionEntry const& other)
                {
                    std::error_code ec;
                    return other.kind == ExtensionKind::Native &&
                        std::filesystem::equivalent(
                            utf8Path(entry.path), utf8Path(other.path), ec);
                });
        }
        if (!known)
        {
            kept.push_back(std::move(entry));
        }
    }
    found = std::move(kept);
}

} // (anon)

Expected<std::vector<ExtensionEntry>>
collectExtensions(
    std::vector<std::string> const& roots,
    report::Reporter& reporter)
{
    std::vector<Candidate> found;
    for (std::string const& root : roots)
    {
        std::string const dir =
            files::makePosixStyle(files::appendPath(root, "extensions"));
        if (!files::exists(dir))
        {
            continue;
        }
        MRDOCS_TRY(collectFromDirectory(dir, found, reporter));
    }
    // A later root replaces the entry of the same name from an earlier one,
    // the way a file in a later addons directory replaces an earlier one.
    std::vector<Candidate> kept;
    kept.reserve(found.size());
    for (Candidate& candidate : found)
    {
        std::string const key = nameKey(candidate.name);
        auto const same = std::ranges::find_if(kept,
            [&](Candidate const& other)
            {
                return nameKey(other.name) == key;
            });
        if (same != kept.end())
        {
            reporter.debug(
                "The extension \"{}\" is replaced by \"{}\"",
                same->entry.path, candidate.entry.path);
            *same = std::move(candidate);
            continue;
        }
        kept.push_back(std::move(candidate));
    }
    // Plain byte order of the extension name.
    std::ranges::sort(kept,
        [](Candidate const& a, Candidate const& b)
        {
            return a.name < b.name;
        });
    std::vector<ExtensionEntry> entries;
    entries.reserve(kept.size());
    for (Candidate& candidate : kept)
    {
        entries.push_back(std::move(candidate.entry));
    }
    removeRepeatedLibraries(entries);
    return entries;
}

Expected<std::vector<ExtensionEntry>>
collectExtensions(Config const& config, report::Reporter& reporter)
{
    return collectExtensions(addonRoots(config), reporter);
}

} // mrdocs
