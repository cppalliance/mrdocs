//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "PluginLoader.hpp"
#include "AddonRoots.hpp"
#include <mrdocs/Generator.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Transform.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/DynamicLibrary.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace mrdocs {

namespace {

// Whether this MrDocs can load a plugin at all. One built without
// `MRDOCS_BUILD_PLUGIN_MODULES`, statically linked for instance, reports a
// plugin it finds rather than handing it to the dynamic loader.
#ifdef MRDOCS_WITHOUT_PLUGINS
constexpr bool canLoadPlugins = false;
#else
constexpr bool canLoadPlugins = true;
#endif

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
// module library the .so extension on macOS, so a plugin there can
// carry either name.
bool
isLibraryName(std::string_view fileName)
{
#ifdef _WIN32
    constexpr std::string_view extensions[] = { ".dll" };
#elif defined(__APPLE__)
    constexpr std::string_view extensions[] = { ".dylib", ".so" };
#else
    constexpr std::string_view extensions[] = { ".so" };
#endif
    return std::ranges::any_of(
        extensions,
        [fileName](std::string_view extension)
        {
            return endsWithExtension(fileName, extension);
        });
}

// Return the libraries directly under `dir`, ordered by name.
std::vector<std::string>
scanPluginDir(std::string_view dir)
{
    std::vector<std::string> found;
    Expected<void> const scanned = forEachFile(dir, false,
        [&found](std::string_view path) -> Expected<void>
        {
            if (files::isRegularFile(path) &&
                isLibraryName(files::getFileName(path)))
            {
                found.emplace_back(path);
            }
            return {};
        });
    if (!scanned)
    {
        // Not a failure of the run: the directory may hold nothing MrDocs
        // wanted. Saying so beats leaving a plugin the user installed out
        // of the run without a word.
        report::warn(
            "The plugin directory \"{}\" could not be read: {}",
            dir, scanned.error().reason());
    }
    std::ranges::sort(found);
    return found;
}

// Return `path`, held in UTF-8 like every path MrDocs reports, as a
// filesystem path. Built from a `std::string` instead, the path would be
// read in the ANSI code page on Windows, and one that is not ASCII would
// name another file.
std::filesystem::path
fromUtf8(std::string_view path)
{
    std::u8string text(path.size(), u8'\0');
    std::ranges::transform(path, text.begin(),
        [](char c)
        {
            return static_cast<char8_t>(c);
        });
    return std::filesystem::path(text);
}

// Append `from` to `to`, leaving out the libraries already there. One
// library can be reached through more than one addon root, and running its
// entry point twice would fail on the id it installed the first time.
// Identity comes from the filesystem, so a root spelled differently, or
// reached through a link, is still recognized.
void
appendNewLibraries(
    std::vector<std::string> const& from,
    std::vector<std::string>& to)
{
    for (std::string const& path : from)
    {
        bool const known = std::ranges::any_of(
            to,
            [&path](std::string const& other)
            {
                std::error_code ec;
                return std::filesystem::equivalent(
                    fromUtf8(path), fromUtf8(other), ec);
            });
        if (!known)
        {
            to.push_back(path);
        }
    }
}

// The context handed to a plugin: it reads the configuration MrDocs
// loaded, forwards a generator to the global registry, and collects the
// transforms for the `PluginRegistry` that `loadPlugins` returns.
class PluginContextImpl final
    : public PluginContext
{
    Config const& config_;
    std::vector<std::unique_ptr<Transform>>& transforms_;

public:
    PluginContextImpl(
        Config const& config,
        std::vector<std::unique_ptr<Transform>>& transforms) noexcept
        : config_(config)
        , transforms_(transforms)
    {
    }

    Config const&
    config() const noexcept override
    {
        return config_;
    }

    Expected<void>
    installGenerator(std::unique_ptr<Generator> G) override
    {
        return mrdocs::installGenerator(std::move(G));
    }

    Expected<void>
    installTransform(std::unique_ptr<Transform> T) override
    {
        MRDOCS_CHECK(T, "cannot install null transform");
        transforms_.push_back(std::move(T));
        return {};
    }
};

// Look a plugin entry point up by name, as a function of type `Fn`.
template <class Fn>
Expected<Fn>
findEntryPoint(
    llvm::sys::DynamicLibrary& library,
    char const* symbol,
    std::string_view path)
{
    void* const address = library.getAddressOfSymbol(symbol);
    MRDOCS_CHECK(address, formatError(
        "the plugin \"{}\" does not export {}", path, symbol));
    return reinterpret_cast<Fn>(address);
}

// Compare the interface the plugin was built against with ours. A
// mismatch means the plugin's view of the context, or of the entry
// points themselves, is not the one it is about to be called with.
Expected<void>
checkApiVersion(
    llvm::sys::DynamicLibrary& library,
    std::string_view path)
{
    MRDOCS_TRY(PluginApiVersionFn const apiVersion,
        findEntryPoint<PluginApiVersionFn>(
            library, "mrdocs_plugin_api_version", path));
    int const version = apiVersion();
    MRDOCS_CHECK(version == MRDOCS_PLUGIN_API_VERSION, formatError(
        "the plugin \"{}\" was built against version {} of the plugin "
        "interface, and this MrDocs provides version {}",
        path, version, MRDOCS_PLUGIN_API_VERSION));
    return {};
}

// Call the entry point and report what the plugin left behind, if it says
// it failed, naming the library.
Expected<void>
runEntryPoint(
    PluginMainFn entryPoint,
    std::string_view path,
    Config const& config,
    std::vector<std::unique_ptr<Transform>>& transforms)
{
    PluginContextImpl context(config, transforms);
    Error error;
    bool const installed = entryPoint(context, &error);
    MRDOCS_CHECK(installed, error.failed()
        ? formatError("the plugin \"{}\" failed: {}", path, error.reason())
        : formatError("the plugin \"{}\" reported a failure", path));
    return {};
}

// Compare the toolchain the plugin was built with against ours. The two
// pass C++ objects between them, so a difference in the compiler, the
// standard library, or how it lays its types out is not something either
// side can survive; refusing here is what keeps it from surfacing as a
// crash somewhere unrelated.
Expected<void>
checkBuildTag(
    llvm::sys::DynamicLibrary& library,
    std::string_view path)
{
    MRDOCS_TRY(PluginBuildTagFn const buildTag,
        findEntryPoint<PluginBuildTagFn>(
            library, "mrdocs_plugin_build_tag", path));
    char const* const tag = buildTag();
    MRDOCS_CHECK(tag, formatError(
        "the plugin \"{}\" reports no toolchain", path));
    MRDOCS_CHECK(std::string_view(tag) == MRDOCS_PLUGIN_BUILD_TAG,
        formatError(
            "the plugin \"{}\" was built with \"{}\", and this MrDocs "
            "with \"{}\"; a plugin has to be built with the toolchain "
            "MrDocs was built with",
            path, tag, MRDOCS_PLUGIN_BUILD_TAG));
    return {};
}

#ifdef _WIN32
// Load the library at `path`, searching the subdirectory named after it,
// such as plugins/stats for plugins/stats.dll, for the libraries it depends
// on. The search then covers the plugin's directory, that subdirectory, the
// system directories and `PATH`. The subdirectory is set for the whole
// process, so it is cleared as soon as the load is done. Return null, with
// the reason in `message`, if the load fails.
HMODULE
loadWithDependencyDir(std::string const& path, std::string& message)
{
    std::error_code ec;
    std::filesystem::path const file =
        std::filesystem::absolute(fromUtf8(path), ec);
    std::filesystem::path const dependencies =
        file.parent_path() / file.stem();
    SetDllDirectoryW(dependencies.c_str());
    HMODULE const handle = LoadLibraryExW(
        file.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (handle == nullptr)
    {
        message = std::system_category().message(
            static_cast<int>(GetLastError()));
    }
    SetDllDirectoryW(nullptr);
    return handle;
}
#endif

// Open the library at `path` for the rest of the run. Elsewhere than on
// Windows, a plugin finds the libraries it depends on through its run path.
llvm::sys::DynamicLibrary
openLibrary(std::string const& path, std::string& message)
{
#ifdef _WIN32
    llvm::sys::DynamicLibrary library;
    HMODULE const handle = loadWithDependencyDir(path, message);
    if (handle != nullptr)
    {
        library = llvm::sys::DynamicLibrary::addPermanentLibrary(
            handle, &message);
    }
    return library;
#else
    return llvm::sys::DynamicLibrary::getPermanentLibrary(
        path.c_str(), &message);
#endif
}

// Load one library and run its entry point.
Expected<void>
loadPlugin(
    std::string const& path,
    Config const& config,
    std::vector<std::unique_ptr<Transform>>& transforms)
{
    std::string message;
    llvm::sys::DynamicLibrary library = openLibrary(path, message);
    MRDOCS_CHECK(library.isValid(), formatError(
        "the plugin \"{}\" could not be loaded: {}", path, message));
    MRDOCS_TRY(checkApiVersion(library, path));
    MRDOCS_TRY(checkBuildTag(library, path));
    MRDOCS_TRY(PluginMainFn const entryPoint,
        findEntryPoint<PluginMainFn>(library, "mrdocs_plugin_main", path));
    MRDOCS_TRY(runEntryPoint(entryPoint, path, config, transforms));
    report::info("Loaded plugin \"{}\"", path);
    return {};
}

} // (anon)

std::vector<std::string>
discoverPlugins(std::vector<std::string> const& roots)
{
    std::vector<std::string> paths;
    for (std::string const& root : roots)
    {
        std::string const dir = files::appendPath(root, "plugins");
        if (files::exists(dir))
        {
            appendNewLibraries(scanPluginDir(dir), paths);
        }
    }
    return paths;
}

PluginRegistry::
PluginRegistry(
    std::vector<std::unique_ptr<Transform>> transforms) noexcept
    : transforms_(std::move(transforms))
{
}

Expected<void>
PluginRegistry::
applyTransforms(Corpus& corpus, Config const& config) const
{
    Expected<void> result;
    for (std::unique_ptr<Transform> const& T : transforms_)
    {
        if (result)
        {
            result = T->apply(corpus, config);
            if (!result)
            {
                result = Unexpected(formatError(
                    "the transform \"{}\" failed: {}",
                    T->id(), result.error().reason()));
            }
        }
    }
    return result;
}

Expected<PluginRegistry>
loadPlugins(Config const& config)
{
    std::vector<std::string> const paths =
        discoverPlugins(addonRoots(config));
    MRDOCS_CHECK(canLoadPlugins || paths.empty(), formatError(
        "the plugin \"{}\" cannot be loaded: this MrDocs was built without "
        "plugin support, which is the case for a statically linked MrDocs "
        "such as the Linux release", paths.front()));
    std::vector<std::unique_ptr<Transform>> transforms;
    for (std::string const& path : paths)
    {
        MRDOCS_TRY(loadPlugin(path, config, transforms));
    }
    return PluginRegistry(std::move(transforms));
}

} // mrdocs
