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
#include <llvm/Support/DynamicLibrary.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>

namespace mrdocs {

namespace {

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
        [fileName](std::string_view const extension)
        {
            return fileName.ends_with(extension);
        });
}

// Return the libraries directly under `dir`, ordered by name.
std::vector<std::string>
scanPluginDir(std::string_view dir)
{
    namespace fs = std::filesystem;
    std::vector<std::string> found;
    std::error_code iterEc;
    fs::directory_iterator const end{};
    for (fs::directory_iterator it(dir, iterEc);
         !iterEc && it != end;
         it.increment(iterEc))
    {
        std::error_code typeEc;
        if (it->is_regular_file(typeEc) &&
            isLibraryName(it->path().filename().string()))
        {
            found.push_back(it->path().string());
        }
    }
    if (iterEc)
    {
        // Not a failure of the run: the directory may hold nothing MrDocs
        // wanted. Saying so beats leaving a plugin the user installed out
        // of the run without a word.
        report::warn(
            "The plugin directory \"{}\" could not be read: {}",
            dir, iterEc.message());
    }
    std::ranges::sort(found);
    return found;
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
                return std::filesystem::equivalent(path, other, ec);
            });
        if (!known)
        {
            to.push_back(path);
        }
    }
}

// The context handed to a plugin: it reads the configuration MrDocs
// loaded and forwards what the plugin installs to the global registry.
class PluginContextImpl final
    : public PluginContext
{
    Config const& config_;

public:
    explicit
    PluginContextImpl(Config const& config) noexcept
        : config_(config)
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
        return mrdocs::installTransform(std::move(T));
    }
};

// Look a plugin entry point up by name.
Expected<void*>
findEntryPoint(
    llvm::sys::DynamicLibrary& library,
    char const* symbol,
    std::string_view path)
{
    void* const address = library.getAddressOfSymbol(symbol);
    MRDOCS_CHECK(address, formatError(
        "the plugin \"{}\" does not export {}", path, symbol));
    return address;
}

// Compare the interface the plugin was built against with ours. A
// mismatch means the plugin's view of the context, or of the entry
// points themselves, is not the one it is about to be called with.
Expected<void>
checkApiVersion(
    llvm::sys::DynamicLibrary& library,
    std::string_view path)
{
    MRDOCS_TRY(void* const address,
        findEntryPoint(library, "mrdocs_plugin_api_version", path));
    int const version =
        reinterpret_cast<PluginApiVersionFn>(address)();
    MRDOCS_CHECK(version == MRDOCS_PLUGIN_API_VERSION, formatError(
        "the plugin \"{}\" was built against version {} of the plugin "
        "interface, and this MrDocs provides version {}",
        path, version, MRDOCS_PLUGIN_API_VERSION));
    return {};
}

// Call the entry point at `address` and report what the plugin left
// behind, if it says it failed.
Expected<void>
runEntryPoint(
    void* address,
    std::string_view path,
    Config const& config)
{
    PluginContextImpl context(config);
    Error error;
    bool const installed =
        reinterpret_cast<PluginMainFn>(address)(context, &error);
    MRDOCS_CHECK(installed, error.failed()
        ? error
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
    MRDOCS_TRY(void* const address,
        findEntryPoint(library, "mrdocs_plugin_build_tag", path));
    char const* const tag =
        reinterpret_cast<PluginBuildTagFn>(address)();
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

// Load one library and run its entry point.
Expected<void>
loadPlugin(
    std::string const& path,
    Config const& config)
{
    std::string message;
    llvm::sys::DynamicLibrary library =
        llvm::sys::DynamicLibrary::getPermanentLibrary(path.c_str(), &message);
    MRDOCS_CHECK(library.isValid(), formatError(
        "the plugin \"{}\" could not be loaded: {}", path, message));
    MRDOCS_TRY(checkApiVersion(library, path));
    MRDOCS_TRY(checkBuildTag(library, path));
    MRDOCS_TRY(void* const address,
        findEntryPoint(library, "mrdocs_plugin_main", path));
    MRDOCS_TRY(runEntryPoint(address, path, config));
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

Expected<void>
loadPlugins(Config const& config)
{
    for (std::string const& path : discoverPlugins(addonRoots(config)))
    {
        MRDOCS_TRY(loadPlugin(path, config));
    }
    return {};
}

} // mrdocs
