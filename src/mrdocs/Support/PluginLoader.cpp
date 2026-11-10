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

#include "PluginLoader.hpp"
#include "../Extensions/ExtensionPaths.hpp"
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Transform.hpp>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
// MRDOCS_STATIC_LIBC is defined when the C library is linked into the
// executable.
#if defined(MRDOCS_STATIC_LIBC) && __has_include(<gnu/libc-version.h>)
#include <gnu/libc-version.h>
#define MRDOCS_STATIC_GLIBC
#endif
#endif

namespace mrdocs {

namespace {

// A native extension as the loader sees it. It holds the library file, the
// directory of the extension when the library sits in one (empty when it
// sits directly in extensions/), and the name the messages give it. That
// name is the path of the library relative to extensions/: `stats.so` for a
// file there, `report/report.so` for a library in the directory `report`.
struct PluginFile
{
    std::string path;
    std::string directory;
    std::string name;
};

PluginFile
makePluginFile(std::string const& path, std::string const& directory)
{
    PluginFile file;
    file.path = path;
    file.directory = directory;
    std::string const fileName = utf8String(utf8Path(path).filename());
    file.name = directory.empty() ? fileName : utf8String(utf8Path(directory).filename()) + "/" + fileName;
    return file;
}

// The transforms and the generators a library installed, in the order the
// entry point installed them.
using PluginRecord = ExtensionRegistry::LibraryRegistry::Record;

// The context handed to a plugin: it reads the configuration MrDocs
// loaded, adds a generator to the registry, and collects the transforms,
// which `loadPlugin` adds to the pipeline once the entry point has
// succeeded. A generator whose id the registry holds is refused. The
// context also records what the plugin installs, for the library registry to
// hand to the next registry that loads the library.
class PluginContextImpl final
    : public PluginContext
{
    Config const& config_;
    ExtensionRegistry& extensions_;
    PluginRecord& record_;

public:
    PluginContextImpl(
        Config const& config,
        ExtensionRegistry& extensions,
        PluginRecord& record) noexcept
        : config_(config)
        , extensions_(extensions)
        , record_(record)
    {
    }

    Config const&
    config() const noexcept override
    {
        return config_;
    }

    using PluginContext::registerGenerator;
    using PluginContext::registerTransform;

    Expected<void>
    registerGenerator(std::unique_ptr<Generator> G) override
    {
        std::shared_ptr<Generator const> generator(std::move(G));
        MRDOCS_TRY(extensions_.generators().install(generator));
        record_.generators.push_back(std::move(generator));
        return {};
    }

    Expected<void>
    registerTransform(std::unique_ptr<Transform> T) override
    {
        MRDOCS_CHECK(T, "cannot register a null transform");
        record_.transforms.push_back(
            std::shared_ptr<Transform const>(std::move(T)));
        return {};
    }
};

#ifdef _WIN32

// Convert the system's message for `code` to UTF-8.
std::string
systemMessage(DWORD const code)
{
    wchar_t* text = nullptr;
    DWORD const length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0,
        reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::string message;
    if (length != 0 && text)
    {
        int const size = ::WideCharToMultiByte(
            CP_UTF8, 0, text, static_cast<int>(length),
            nullptr, 0, nullptr, nullptr);
        if (size > 0)
        {
            message.resize(static_cast<std::size_t>(size));
            ::WideCharToMultiByte(
                CP_UTF8, 0, text, static_cast<int>(length),
                message.data(), size, nullptr, nullptr);
        }
        while (!message.empty() &&
            (message.back() == '\n' || message.back() == '\r' ||
                message.back() == ' '))
        {
            message.pop_back();
        }
    }
    if (text)
    {
        ::LocalFree(text);
    }
    if (message.empty())
    {
        message = "error " + std::to_string(code);
    }
    return message;
}

#endif

// A loaded library. The loader opens libraries for the life of the
// process and never closes them. The generators and transforms a plugin
// installs live in registries that are destroyed after main returns, and
// their code has to still be mapped when that happens.
using LibraryHandle = void*;

// Open the library at `path`. Every symbol it needs is resolved now, so a
// plugin that refers to something MrDocs does not provide is refused here,
// instead of failing at its first call. The POSIX loader's message names the
// missing symbol. The Windows one only says a module or procedure was not
// found. Its symbols stay local to it, so two plugins cannot interpose each
// other's C++ symbols.
Expected<LibraryHandle>
openLibrary(
    PluginFile const& file,
    [[maybe_unused]] report::Reporter& reporter)
{
#ifdef _WIN32
    std::error_code ec;
    std::filesystem::path const absolute =
        std::filesystem::absolute(utf8Path(file.path), ec);
    MRDOCS_CHECK(!ec, formatError("the plugin \"{}\" could not be loaded: {}", file.name, ec.message()));
    // The libraries the plugin depends on live next to it, in the directory
    // of its extension. That directory joins the search for the duration of
    // this load only. The search then covers the directory of the plugin,
    // the added one, the directory of the executable and the system
    // directories. It does not cover the current directory or PATH. A
    // plugin directly in extensions/ has no directory of its own to add:
    // the directory of the plugin is already searched.
    DLL_DIRECTORY_COOKIE cookie = nullptr;
    if (!file.directory.empty())
    {
        std::filesystem::path const dependencies =
            std::filesystem::absolute(utf8Path(file.directory), ec);
        cookie = ec ? nullptr : ::AddDllDirectory(dependencies.c_str());
        if (!cookie)
        {
            reporter.warn(
                "The libraries directory of the plugin \"{}\" could not be "
                "added to the search path: {}",
                file.name,
                ec ? ec.message() : systemMessage(::GetLastError()));
        }
    }
    HMODULE const handle = ::LoadLibraryExW(
        absolute.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    DWORD const error = handle ? 0 : ::GetLastError();
    if (cookie)
    {
        ::RemoveDllDirectory(cookie);
    }
    MRDOCS_CHECK(handle, formatError("the plugin \"{}\" could not be loaded: {}", file.name, systemMessage(error)));
    return reinterpret_cast<LibraryHandle>(handle);
#else
    // Clear any error left over from an earlier call, so the text read
    // below is this call's.
    ::dlerror();
    LibraryHandle const handle = ::dlopen(
        file.path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
        char const* const text = ::dlerror();
        std::string message = text ? text : "unknown error";
#ifdef MRDOCS_STATIC_GLIBC
        // A static glibc loads a library with the shared glibc of the system,
        // which has to be the version this MrDocs was linked with.
        message += std::format(
            ". This MrDocs is linked statically against glibc {}, so it loads "
            "plugins only on a system whose shared glibc is that same version",
            ::gnu_get_libc_version());
#endif
        return Unexpected(formatError(
            "the plugin \"{}\" could not be loaded: {}",
            file.name, message));
    }
    return handle;
#endif
}

// Return the address of `symbol` in `library`, or null if it has none.
void*
findSymbol(LibraryHandle library, char const* symbol)
{
#ifdef _WIN32
    return reinterpret_cast<void*>(::GetProcAddress(
        static_cast<HMODULE>(library), symbol));
#else
    return ::dlsym(library, symbol);
#endif
}

// Look a plugin entry point up by name, as a function of type `Fn`.
template <class Fn>
Expected<Fn>
findEntryPoint(
    LibraryHandle library,
    char const* symbol,
    std::string_view name)
{
    void* const address = findSymbol(library, symbol);
    MRDOCS_CHECK(address, formatError("the plugin \"{}\" does not export {}", name, symbol));
    return reinterpret_cast<Fn>(address);
}

// Compare the MrDocs build the plugin was compiled against with ours. The
// layout of the types a plugin sees changes in most commits, so only the
// exact build is accepted. A build without an id has nothing to compare, so
// it accepts no plugin.
Expected<void>
checkBuildId(
    LibraryHandle library,
    std::string_view name)
{
    MRDOCS_CHECK(!std::string_view(MRDOCS_PLUGIN_BUILD_ID).empty(),
        formatError(
            "this MrDocs has no build id (it was built without git "
            "information), so it cannot check that the plugin \"{}\" was "
            "built against it; set -DMRDOCS_BUILD_ID=<id> when building "
            "MrDocs from a source archive",
            name));
    MRDOCS_TRY(PluginBuildIdFn const buildId,
        findEntryPoint<PluginBuildIdFn>(
            library, "mrdocs_plugin_build_id", name));
    char const* const id = buildId();
    MRDOCS_CHECK(id, formatError("the plugin \"{}\" reports no build id", name));
    MRDOCS_CHECK(std::string_view(id) == MRDOCS_PLUGIN_BUILD_ID,
        formatError(
            "the plugin \"{}\" was built for MrDocs build {}, and this is "
            "build {}; rebuild it against this MrDocs",
            name, id, MRDOCS_PLUGIN_BUILD_ID));
    return {};
}

// Call the entry point and report what the plugin left behind, if it says
// it failed, naming the library.
Expected<void>
runEntryPoint(
    PluginMainFn entryPoint,
    std::string_view name,
    Config const& config,
    ExtensionRegistry& extensions,
    PluginRecord& record)
{
    PluginContextImpl context(config, extensions, record);
    Error error;
    bool const installed = entryPoint(context, &error);
    MRDOCS_CHECK(installed, error.failed() ? formatError("the plugin \"{}\" failed: {}", name, error.reason()) : formatError("the plugin \"{}\" reported a failure", name));
    return {};
}

// Compare the standard library the plugin was built with against ours. The
// two pass C++ objects between them, so neither side can survive a
// difference in the library or in how it lays its types out. Refusing here
// keeps the difference from surfacing as a crash somewhere unrelated. The
// compiler is not compared.
Expected<void>
checkStdlibTag(
    LibraryHandle library,
    std::string_view name)
{
    MRDOCS_TRY(PluginStdlibTagFn const stdlibTag,
        findEntryPoint<PluginStdlibTagFn>(
            library, "mrdocs_plugin_stdlib_tag", name));
    char const* const tag = stdlibTag();
    MRDOCS_CHECK(tag, formatError("the plugin \"{}\" reports no standard library", name));
    MRDOCS_CHECK(std::string_view(tag) == MRDOCS_PLUGIN_STDLIB_TAG,
        formatError(
            "the plugin \"{}\" was built with {}, and this MrDocs "
            "with {}; rebuild it with the same standard library and "
            "settings",
            name, tag, MRDOCS_PLUGIN_STDLIB_TAG));
    return {};
}

// The identity of the library at `path` among the ones loaded: the same file
// reached through another root, or through a link, has the same key.
std::string
libraryKey(std::string const& path)
{
    std::error_code ec;
    std::filesystem::path const canonical =
        std::filesystem::weakly_canonical(utf8Path(path), ec);
    if (ec)
    {
        return path;
    }
    // The inverse of utf8Path: generic_string() would convert through the
    // ANSI code page on Windows, and throw for a character it lacks.
    std::u8string const text = canonical.generic_u8string();
    return std::string(text.begin(), text.end());
}

} // (anon)

Expected<void>
loadPlugin(
    std::string const& path,
    std::string const& directory,
    Config const& config,
    ExtensionRegistry& extensions,
    ExtensionRegistry::LibraryRegistry& libraries,
    report::Reporter& reporter)
{
    PluginFile const file = makePluginFile(path, directory);
    std::string const key = libraryKey(path);
    std::lock_guard<std::mutex> const lock(libraries.mutex_);
    if (auto const known = libraries.byPath_.find(key);
        known != libraries.byPath_.end())
    {
        for (std::shared_ptr<Transform const> const& T :
            known->second.transforms)
        {
            MRDOCS_TRY(extensions.installTransform(T));
        }
        for (std::shared_ptr<Generator const> const& G :
            known->second.generators)
        {
            MRDOCS_TRY(extensions.generators().install(G));
        }
        return {};
    }
    MRDOCS_TRY(LibraryHandle const library, openLibrary(file, reporter));
    MRDOCS_TRY(checkBuildId(library, file.name));
    MRDOCS_TRY(checkStdlibTag(library, file.name));
    MRDOCS_TRY(PluginMainFn const entryPoint,
        findEntryPoint<PluginMainFn>(
            library, "mrdocs_plugin_main", file.name));
    PluginRecord record;
    MRDOCS_TRY(runEntryPoint(
        entryPoint, file.name, config, extensions, record));
    for (std::shared_ptr<Transform const> const& T : record.transforms)
    {
        MRDOCS_TRY(extensions.installTransform(T));
    }
    libraries.byPath_.emplace(key, std::move(record));
    reporter.info("Loaded plugin \"{}\"", file.name);
    return {};
}

} // mrdocs
