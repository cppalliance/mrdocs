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

// The interface a shared library implements to extend MrDocs from
// outside the tool.

#ifndef MRDOCS_API_PLUGIN_HPP
#define MRDOCS_API_PLUGIN_HPP

// The standard library check comes right after Platform.hpp, which defines
// the tag, and before every other MrDocs header: with the old libstdc++ ABI
// those headers fail to compile, and the compiler reports errors in the
// order it meets them.
#include <mrdocs/Platform.hpp>
#include <string_view>

// `MRDOCS_EXPECTED_STDLIB_TAG` is the standard library tag of the MrDocs a
// plugin is built for, a string literal the build of the plugin defines. The
// `mrdocs::plugin-api` CMake target defines it with the value
// `MRDOCS_PLUGIN_STDLIB_TAG` had when MrDocs was built. When it is defined,
// a plugin compiled with other standard library settings stops at its first
// source file, instead of at load time. A plugin that does not define it is
// checked when MrDocs loads it.
#if defined(MRDOCS_EXPECTED_STDLIB_TAG)
static_assert(
    std::string_view(MRDOCS_PLUGIN_STDLIB_TAG) == MRDOCS_EXPECTED_STDLIB_TAG,
    "the standard library settings of this plugin differ from the ones "
    "MrDocs was built with: this plugin has " MRDOCS_PLUGIN_STDLIB_TAG
    " and MrDocs has " MRDOCS_EXPECTED_STDLIB_TAG
    "; rebuild the plugin with the same standard library and settings");
#endif

#include <mrdocs/Config.hpp>
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Transform.hpp>
#include <mrdocs/Version.hpp>
#include <mrdocs/detail/CallableGenerator.hpp>
#include <mrdocs/detail/CallableTransform.hpp>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

/** The identity of the MrDocs build a plugin is compiled against.

    A plugin sees MrDocs types by their layout, and that layout changes
    in most commits, so a plugin loads only into the exact build it was
    compiled against. A plugin reports this value through
    `mrdocs_plugin_build_id`, and MrDocs refuses one whose value differs
    from its own instead of letting it fail later, somewhere unrelated.

    The text is the one of `mrdocs::project_build_id`: the first 12 digits
    of the commit, such as `662773e585c8`, with `.modified` after them for a
    tree with edits.
*/
#define MRDOCS_PLUGIN_BUILD_ID MRDOCS_PROJECT_BUILD_ID

#if defined(_MSC_VER)
#define MRDOCS_DETAIL_PLUGIN_EXPORT __declspec(dllexport)
#else
#define MRDOCS_DETAIL_PLUGIN_EXPORT \
    __attribute__((__visibility__("default")))
#endif

/** The attribute that makes a plugin's entry points visible.

    It expands to `__declspec(dllexport)` on MSVC and to
    `__attribute__((__visibility__("default")))` elsewhere. MrDocs looks
    the entry points up by name in the loaded library, so they have to be
    exported from it. The plugin recipe compiles a plugin with hidden
    visibility, and Windows exports nothing that lacks the attribute, so
    an entry point without it is invisible to the loader.

    @ref MRDOCS_PLUGIN_MAIN applies it to the functions it defines. A
    plugin that does not use that macro puts it before the return type of
    each of the three `extern "C"` entry points, `mrdocs_plugin_build_id`,
    `mrdocs_plugin_stdlib_tag` and `mrdocs_plugin_main`:

    @code
    extern "C" MRDOCS_PLUGIN_EXPORT char const*
    mrdocs_plugin_build_id()
    {
        return MRDOCS_PLUGIN_BUILD_ID;
    }

    extern "C" MRDOCS_PLUGIN_EXPORT char const*
    mrdocs_plugin_stdlib_tag()
    {
        return MRDOCS_PLUGIN_STDLIB_TAG;
    }

    extern "C" MRDOCS_PLUGIN_EXPORT bool
    mrdocs_plugin_main(mrdocs::PluginContext& plugin, mrdocs::Error* error)
    {
        // Register generators and transforms, and fill in *error on failure.
        return true;
    }
    @endcode

    The attribute is for the entry points only. The other functions of a
    plugin stay hidden on purpose, and `MRDOCS_DECL` marks the MrDocs API,
    not plugin code.

    Without it the library loads but the loader cannot find the entry
    points, and the run stops with `the plugin "<name>" does not export
    mrdocs_plugin_build_id`, the first of the three it looks for.
*/
#define MRDOCS_PLUGIN_EXPORT MRDOCS_DETAIL_PLUGIN_EXPORT

namespace mrdocs {

/** The interface a plugin uses to extend MrDocs.

    MrDocs creates a context and passes it to the plugin entry point,
    which registers through it whatever the plugin provides. The
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
        plugin is called, so a plugin can decide what to register from
        the settings the user wrote.

        A plugin's entry point runs once per @ref
        ExtensionRegistry::LibraryRegistry (the `mrdocs` tool has one for the
        whole process), with the configuration of the first run that loads it.
        A later run that shares the library registry reuses what the plugin
        registered. A choice that depends on the run belongs in @ref
        Transform::apply, which receives the configuration of every
        run and reads its options with @ref getTransformParams. The
        reference is valid for the duration of the call, so a plugin must
        not keep it.
    */
    virtual Config const&
    config() const noexcept = 0;

    /** Register a generator.

        The generator becomes selectable under its own id, on the
        command line and in the configuration, alongside the ones that
        ship with MrDocs.

        @return An error if the generator is null, or if one with the
        same id is already registered.

        @param G The generator to register. Ownership is transferred to
        MrDocs, which keeps it while a registry or the library registry
        holds it. The library stays loaded for the rest of the process.
    */
    virtual Expected<void>
    registerGenerator(std::unique_ptr<Generator> G) = 0;

    /** Register a corpus transform.

        The transform joins the pipeline of the run, with the transforms
        that pipeline extension scripts register. The order is not
        something a transform may rely on: it works on whatever corpus it
        is handed and must not assume another transform ran before it or
        will run after it. It reads the `transform-options.<id>` block of
        its @ref Transform::id with @ref getTransformParams.

        @return An error if the transform is null.

        @param T The transform to register. Ownership is transferred to
        MrDocs, which keeps it while a registry or the library registry
        holds it and shares it with any later run that loads the same
        plugin through the same library registry. The library stays loaded
        for the rest of the process.
    */
    virtual Expected<void>
    registerTransform(std::unique_ptr<Transform> T) = 0;

    /** Register a generator from an id and a callable.

        The callable is the generator's whole job, the way the function a
        script passes to `mrdocs.register_generator` is. It is called with a
        @ref GeneratorExtensionContext each time the generator runs, and
        returns an error to stop the run. MrDocs wraps it in a @ref Generator whose
        @ref Generator::id is `id` and registers that with the overload
        above, so the same rules apply: the generator is selectable under
        `id`, and one with that id already registered is refused.

        The callable is copied for each call, so state it captures does not
        carry from one call to the next and it has to be copy constructible;
        use the overload above for a generator that keeps state across calls.
        The callable must not throw: it reports a failure by returning an
        error, the way the generator does.

        @code
        return context.registerGenerator("notes",
            [](mrdocs::GeneratorExtensionContext const& ctx) -> mrdocs::Expected<void>
            {
                // write files under ctx.outputPath
                return {};
            });
        @endcode

        @return An error if a generator with the same id is already
        registered.

        @param id The generator's id.
        @param fn A callable taking a @ref GeneratorExtensionContext const& and
        returning `Expected<void>`.
        @param displayName The generator's display name; `id` when empty.
        @param fileExtension The generator's file extension, without a leading
        period; empty by default.
    */
    template <class F>
    requires std::copy_constructible<std::decay_t<F>> &&
        std::is_invocable_r_v<
            Expected<void>,
            std::decay_t<F>&,
            GeneratorExtensionContext const&>
    Expected<void>
    registerGenerator(
        std::string_view id,
        F&& fn,
        std::string_view displayName = {},
        std::string_view fileExtension = {})
    {
        return registerGenerator(
            std::make_unique<detail::CallableGenerator<std::decay_t<F>>>(
                id, displayName, fileExtension, std::forward<F>(fn)));
    }

    /** Register a corpus transform from an id and a callable.

        The callable is the transform's whole job, the way the function a
        script passes to `mrdocs.register_transform` is. It is called with a
        @ref TransformExtensionContext each time the pipeline runs, and
        returns an error to stop the run. MrDocs wraps it in a @ref Transform whose
        @ref Transform::id is `id` and registers that with the overload
        above, so the transform joins the pipeline the same way and receives
        the `transform-options.<id>` block as `params`.

        The callable is copied for each call, so state it captures does not
        carry from one call to the next and it has to be copy constructible;
        use the overload above for a transform that keeps state across calls.
        The callable must not throw: it reports a failure by returning an
        error, the way the transform does.

        @code
        return context.registerTransform("tidy",
            [](mrdocs::TransformExtensionContext const& ctx) -> mrdocs::Expected<void>
            {
                for (mrdocs::Symbol const& symbol : *ctx.corpus)
                {
                    mrdocs::Symbol* target = ctx.corpus->find(symbol.id);
                    // read or change the symbol through target
                }
                return {};
            });
        @endcode

        @return An error if the registration is refused.

        @param id The transform's id.
        @param fn A callable taking a @ref TransformExtensionContext const& and
        returning `Expected<void>`.
    */
    template <class F>
    requires std::copy_constructible<std::decay_t<F>> &&
        std::is_invocable_r_v<
            Expected<void>,
            std::decay_t<F>&,
            TransformExtensionContext const&>
    Expected<void>
    registerTransform(std::string_view id, F&& fn)
    {
        return registerTransform(
            std::make_unique<detail::CallableTransform<std::decay_t<F>>>(
                id, std::forward<F>(fn)));
    }
};

/** The type of the build identity function a plugin exports.

    The function is `extern "C"`, exported with @ref MRDOCS_PLUGIN_EXPORT,
    and named `mrdocs_plugin_build_id`. It returns
    @ref MRDOCS_PLUGIN_BUILD_ID as the plugin saw it, in storage that
    outlives the call.
*/
using PluginBuildIdFn = char const* (*)();

/** The type of the standard library function a plugin exports.

    The function is `extern "C"`, exported with @ref MRDOCS_PLUGIN_EXPORT,
    and named `mrdocs_plugin_stdlib_tag`. It returns
    @ref MRDOCS_PLUGIN_STDLIB_TAG as the plugin saw it, in storage that
    outlives the call.
*/
using PluginStdlibTagFn = char const* (*)();

/** The type of the entry point a plugin exports.

    The function is `extern "C"`, exported with @ref MRDOCS_PLUGIN_EXPORT,
    and named `mrdocs_plugin_main`. MrDocs calls it once, while it starts
    up, with a context and the address of an error to fill in. A plugin
    that returns `false` stops the run, reporting the error it left
    behind.

    A plugin defines the entry point with @ref MRDOCS_PLUGIN_MAIN rather
    than writing this signature out: the body of the macro returns an
    `Expected<void>`, which the macro turns into the two.
*/
using PluginMainFn = bool (*)(PluginContext&, Error*);

} // mrdocs

/** Define the entry point of a plugin.

    Use it once per plugin library, in exactly one translation unit, at
    namespace scope. It is the only thing a plugin has to define. A plugin
    may instead define the three `extern "C"` functions itself, in C++,
    with the signatures
    @ref mrdocs::PluginBuildIdFn, @ref mrdocs::PluginStdlibTagFn and
    @ref mrdocs::PluginMainFn, each exported with @ref MRDOCS_PLUGIN_EXPORT.

    The macro defines `mrdocs_plugin_build_id` and
    `mrdocs_plugin_stdlib_tag`, which return @ref MRDOCS_PLUGIN_BUILD_ID
    and @ref MRDOCS_PLUGIN_STDLIB_TAG as the plugin's compiler saw them,
    and `mrdocs_plugin_main`, which calls the body that follows the macro.
    The body has the signature
    `static mrdocs::Expected<void> installMrdocsPlugin(mrdocs::PluginContext& plugin)`,
    with `plugin` named by the macro argument.

    The parameter is a @ref mrdocs::PluginContext. Through it the body
    reads `config()` and calls `registerGenerator` and `registerTransform`.
    The reference is valid during the call only.

    The body returns `{}` on success. To refuse, it returns
    `mrdocs::Unexpected(mrdocs::formatError(...))`, which stops the run with
    `the plugin "<name>" failed: <message>`. `MRDOCS_TRY` works in the body,
    because the error of a failed registration is returned the same way. The
    entry point runs once per @ref mrdocs::ExtensionRegistry::LibraryRegistry,
    before the corpus is built. The tool has one per process, and a program
    that runs more than once shares one among its sessions. Work that depends
    on the run belongs in @ref mrdocs::Transform::apply or @ref
    mrdocs::Generator::build.

    The body must not keep the context reference or a pointer to the
    configuration. It must not throw either: the macro does not catch
    exceptions, and the `Expected` the body returns is the only channel,
    so turn failures into an @ref mrdocs::Error.

    Register a generator:

    @code
    MRDOCS_PLUGIN_MAIN(plugin)
    {
        return plugin.registerGenerator(std::make_unique<MyGenerator>());
    }
    @endcode

    Register a transform and a generator. A registration that fails, such
    as an id that is already taken, refuses the plugin with its message:

    @code
    MRDOCS_PLUGIN_MAIN(plugin)
    {
        mrdocs::Expected<void> added = plugin.registerTransform(
            std::make_unique<MyTransform>());
        if (!added)
        {
            return added;
        }
        added = plugin.registerGenerator(std::make_unique<MyGenerator>());
        if (!added)
        {
            return added;
        }
        return {};
    }
    @endcode

    @param plugin The name the body uses for the context it receives.
*/
#define MRDOCS_PLUGIN_MAIN(plugin) \
    static ::mrdocs::Expected<void> \
    installMrdocsPlugin(::mrdocs::PluginContext&); \
\
    extern "C" MRDOCS_PLUGIN_EXPORT char const* \
    mrdocs_plugin_build_id() \
    { \
        return MRDOCS_PLUGIN_BUILD_ID; \
    } \
\
    extern "C" MRDOCS_PLUGIN_EXPORT char const* \
    mrdocs_plugin_stdlib_tag() \
    { \
        return MRDOCS_PLUGIN_STDLIB_TAG; \
    } \
\
    extern "C" MRDOCS_PLUGIN_EXPORT bool \
    mrdocs_plugin_main( \
        ::mrdocs::PluginContext& mrdocsContext, \
        ::mrdocs::Error* mrdocsError) \
    { \
        ::mrdocs::Expected<void> const mrdocsResult = \
            installMrdocsPlugin(mrdocsContext); \
        if (!mrdocsResult) \
        { \
            *mrdocsError = mrdocsResult.error(); \
        } \
        return mrdocsResult.has_value(); \
    } \
\
    static ::mrdocs::Expected<void> \
    installMrdocsPlugin(::mrdocs::PluginContext& plugin)

#endif // MRDOCS_API_PLUGIN_HPP
