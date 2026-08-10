//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The interface a shared library implements to extend MrDocs from
// outside the tool.

#ifndef MRDOCS_API_PLUGIN_HPP
#define MRDOCS_API_PLUGIN_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <memory>

/** The version of the interface a plugin is built against.

    A plugin exports `mrdocs_plugin_api_version`, which returns the
    value of this macro as it was defined when the plugin was compiled.
    MrDocs calls the plugin only when that value matches its own, so a
    plugin built against a different interface is reported as an error
    instead of being called with the wrong expectations.

    The value changes whenever the plugin interface changes, which means
    every plugin has to be rebuilt against the new headers.
*/
#define MRDOCS_PLUGIN_API_VERSION 1

#define MRDOCS_PLUGIN_STRINGIZE_(x) #x
#define MRDOCS_PLUGIN_STRINGIZE(x) MRDOCS_PLUGIN_STRINGIZE_(x)

#if defined(_MSC_VER)
#    define MRDOCS_PLUGIN_COMPILER_TAG \
         "msvc " MRDOCS_PLUGIN_STRINGIZE(_MSC_VER)
#elif defined(__clang__)
#    define MRDOCS_PLUGIN_COMPILER_TAG \
         "clang " MRDOCS_PLUGIN_STRINGIZE(__clang_major__)
#elif defined(__GNUC__)
#    define MRDOCS_PLUGIN_COMPILER_TAG \
         "gcc " MRDOCS_PLUGIN_STRINGIZE(__GNUC__)
#else
#    define MRDOCS_PLUGIN_COMPILER_TAG "unrecognized compiler"
#endif

#if defined(_DLL)
#    define MRDOCS_PLUGIN_CRT_TAG "dynamic"
#else
#    define MRDOCS_PLUGIN_CRT_TAG "static"
#endif

#if defined(_GLIBCXX_DEBUG)
#    define MRDOCS_PLUGIN_GLIBCXX_DEBUG_TAG "on"
#else
#    define MRDOCS_PLUGIN_GLIBCXX_DEBUG_TAG "off"
#endif

// Alongside the standard library itself, only the settings that change how
// its types are laid out, or which heap they allocate from: the MSVC
// iterator debug level and C runtime, the libc++ ABI version, and the
// libstdc++ debug mode. Assertion and hardening settings are left out, as
// they change what the inline code checks rather than what it operates on,
// and a plugin should not have to match them.
#if defined(_MSVC_STL_UPDATE)
#    define MRDOCS_PLUGIN_STDLIB_TAG                                \
         "msvc-stl " MRDOCS_PLUGIN_STRINGIZE(_MSVC_STL_UPDATE)      \
         ", iterator-debug-level "                                  \
         MRDOCS_PLUGIN_STRINGIZE(_ITERATOR_DEBUG_LEVEL)             \
         ", crt " MRDOCS_PLUGIN_CRT_TAG
#elif defined(_LIBCPP_VERSION)
#    define MRDOCS_PLUGIN_STDLIB_TAG                                \
         "libc++ " MRDOCS_PLUGIN_STRINGIZE(_LIBCPP_VERSION)         \
         ", abi " MRDOCS_PLUGIN_STRINGIZE(_LIBCPP_ABI_VERSION)
#elif defined(_GLIBCXX_RELEASE)
#    define MRDOCS_PLUGIN_STDLIB_TAG                                \
         "libstdc++ " MRDOCS_PLUGIN_STRINGIZE(_GLIBCXX_RELEASE)     \
         ", debug " MRDOCS_PLUGIN_GLIBCXX_DEBUG_TAG
#else
#    define MRDOCS_PLUGIN_STDLIB_TAG "unrecognized standard library"
#endif

/** The toolchain a plugin was built with.

    A plugin and MrDocs pass C++ objects between them: the plugin
    allocates a generator that MrDocs destroys, and both inline the
    standard library types the API exposes. That holds together only when
    the two were built with the same compiler and standard library, in a
    configuration that lays those types out the same way. A plugin
    reports this through `mrdocs_plugin_build_tag`, so that MrDocs can
    refuse one whose toolchain differs rather than fail later, somewhere
    unrelated.
*/
#define MRDOCS_PLUGIN_BUILD_TAG \
    MRDOCS_PLUGIN_COMPILER_TAG ", " MRDOCS_PLUGIN_STDLIB_TAG

/** The attribute that makes a plugin's entry points visible.

    MrDocs looks the entry points up by name in the loaded library, so
    they have to be exported from it.
*/
#if defined(_MSC_VER)
#    define MRDOCS_PLUGIN_EXPORT __declspec(dllexport)
#else
#    define MRDOCS_PLUGIN_EXPORT __attribute__((__visibility__("default")))
#endif

namespace mrdocs {

/** The interface a plugin uses to extend MrDocs.

    MrDocs creates a context and passes it to the plugin entry point,
    which installs through it whatever the plugin provides. The
    reference is valid for the duration of that call only.
*/
class MRDOCS_VISIBLE
    PluginContext
{
protected:
    /** Destructor.

        MrDocs owns the context, so a plugin never destroys it.
    */
    ~PluginContext() noexcept = default;

public:
    /** Return the configuration MrDocs is running with.

        The configuration is already loaded and normalized when the
        plugin is called, so a plugin can decide what to install from
        the settings the user wrote.
    */
    virtual
    Config const&
    config() const noexcept = 0;

    /** Install a generator.

        The generator becomes selectable under its own id, on the
        command line and in the configuration, alongside the ones that
        ship with MrDocs.

        @return An error if the generator is null, or if one with the
        same id is already installed.

        @param G The generator to install. Ownership is transferred to
        MrDocs, which keeps it for the rest of the run.
    */
    virtual
    Expected<void>
    installGenerator(std::unique_ptr<Generator> G) = 0;
};

/** The type of the version function a plugin exports.

    The function is `extern "C"` and named `mrdocs_plugin_api_version`.
    It returns @ref MRDOCS_PLUGIN_API_VERSION as the plugin saw it.
*/
using PluginApiVersionFn = int (*)();

/** The type of the toolchain function a plugin exports.

    The function is `extern "C"` and named `mrdocs_plugin_build_tag`. It
    returns @ref MRDOCS_PLUGIN_BUILD_TAG as the plugin saw it, in storage
    that outlives the call.
*/
using PluginBuildTagFn = char const* (*)();

/** The type of the entry point a plugin exports.

    The function is `extern "C"` and named `mrdocs_plugin_main`. MrDocs
    calls it once, while it starts up, with a context and the address of
    an error to fill in. A plugin that returns `false` stops the run,
    reporting the error it left behind.

    A plugin defines the entry point with @ref MRDOCS_PLUGIN_MAIN rather
    than writing this signature out: the body of the macro returns an
    `Expected<void>`, which the macro turns into the two.
*/
using PluginMainFn = bool (*)(PluginContext&, Error*);

/** Load the plugins the configuration makes visible.

    Each addon root contributes the libraries directly under its
    plugins subdirectory, in root order and then by name within a
    root, and one reachable through more than one root is taken once.
    Every one of them is loaded for the lifetime of the process
    and its entry point is called once, so that what a plugin installs
    is in place before anything looks for it.

    Call this before a generator is looked up by id with
    @ref findGenerator. It is one of the pieces the command-line tool
    composes to run its generate step; the order of that step lives in
    the tool.

    A library that cannot be loaded, does not export the entry points,
    was built against another version of the plugin interface or with
    another toolchain, or reports an error of its own fails the call: a
    plugin is there because the user put it there, so one that does
    nothing is not silently accepted.

    @par Thread Safety
    The registries this installs into are synchronized, so a concurrent
    @ref installGenerator is safe. Two concurrent calls to this function
    are not: both would run the same entry points, and the second
    installation of a generator id fails.

    @return The error, if any occurred.

    @param config The resolved configuration whose addon roots are
    walked, and which the plugins read.
*/
MRDOCS_DECL
Expected<void>
loadPlugins(Config const& config);

} // mrdocs

/** Define the entry point of a plugin.

    Name the @ref mrdocs::PluginContext parameter and write a body that
    returns what came of the work:

    @code
    MRDOCS_PLUGIN_MAIN(ctx)
    {
        return ctx.installGenerator(std::make_unique<MyGenerator>());
    }
    @endcode

    Along with the entry point, which passes the error the body returned
    back to MrDocs, the macro defines the functions reporting the
    interface version and the toolchain the plugin was built with, so
    that none of the three can disagree with each other.

    @param context The name the body uses for the context it receives.
*/
#define MRDOCS_PLUGIN_MAIN(context)                                 \
    static ::mrdocs::Expected<void>                                 \
    mrdocsPluginMain(::mrdocs::PluginContext&);                     \
                                                                    \
    extern "C" MRDOCS_PLUGIN_EXPORT int                             \
    mrdocs_plugin_api_version()                                     \
    {                                                               \
        return MRDOCS_PLUGIN_API_VERSION;                           \
    }                                                               \
                                                                    \
    extern "C" MRDOCS_PLUGIN_EXPORT char const*                     \
    mrdocs_plugin_build_tag()                                       \
    {                                                               \
        return MRDOCS_PLUGIN_BUILD_TAG;                             \
    }                                                               \
                                                                    \
    extern "C" MRDOCS_PLUGIN_EXPORT bool                            \
    mrdocs_plugin_main(                                             \
        ::mrdocs::PluginContext& mrdocsContext,                     \
        ::mrdocs::Error* mrdocsError)                               \
    {                                                               \
        ::mrdocs::Expected<void> const mrdocsResult =               \
            mrdocsPluginMain(mrdocsContext);                        \
        if (!mrdocsResult)                                          \
        {                                                           \
            *mrdocsError = mrdocsResult.error();                    \
        }                                                           \
        return mrdocsResult.has_value();                            \
    }                                                               \
                                                                    \
    static ::mrdocs::Expected<void>                                 \
    mrdocsPluginMain(::mrdocs::PluginContext& context)

#endif // MRDOCS_API_PLUGIN_HPP
