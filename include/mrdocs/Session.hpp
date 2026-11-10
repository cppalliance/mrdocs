//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SESSION_HPP
#define MRDOCS_API_SESSION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Extensions/GeneratorRegistry.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <memory>

namespace mrdocs {

class Corpus;

/** Everything one run of MrDocs owns.

    A session is made from the configuration of a run with @ref create and
    destroyed when the run ends. It owns that configuration, returned by
    @ref config. A session that exists has loaded its extensions. It owns the
    @ref ExtensionRegistry, which in turn owns the @ref GeneratorRegistry. The generators, the transform pipeline and the
    scripting engines of the run live and die together, and nothing refers to
    them from outside. The session owns and forwards. The documentation of
    each piece says what it does.

    One run owns one session. The only piece that sessions may share is the
    @ref ExtensionRegistry::LibraryRegistry. A shared library cannot be
    unloaded and the entry point of a plugin must not run twice, so a program
    that runs more than once in a process, such as a test runner, constructs
    one and passes it to each @ref create. A session created alone has one of
    its own.

    The session also owns the @ref report::Reporter of the run, returned by
    @ref log. It starts from `log-level` and `warn-as-error`. The transforms
    and generators the session runs report through it, and so does a corpus
    built with it, so two sessions never share a level or a total.

    A corpus given to @ref applyTransforms must outlive the session,
    because a script may keep the corpus it was handed. So the corpus is
    declared before the session and destroyed after it.

    A session can be moved. The references that @ref log, @ref generators
    and @ref extensions returned stay valid.

    @code
    Config config = ...;
    // The corpus is declared before the session, so it is destroyed after
    std::optional<Corpus> corpus;
    MRDOCS_TRY(Session session, Session::create(std::move(config)));
    MRDOCS_TRY(Corpus built, Corpus::build(session.config(), session.log()));
    corpus.emplace(std::move(built));
    MRDOCS_TRY(session.applyTransforms(*corpus));
    Generator const* generator = session.generators().find(id);
    MRDOCS_TRY(session.build(*generator, *corpus));
    @endcode
*/
class MRDOCS_DECL
    Session
{
public:
    /** Make the session of a run.

        The session starts with the generators that ship with MrDocs. The
        reporter takes its level and `warn-as-error` from the configuration.
        The extensions of the configuration load first, so that a generator
        one of them installs takes precedence over an addon directory of the
        same id. Then the generators the addons define as data are
        discovered.

        @param config The configuration of the run. The session owns it.
        @param libraries The record of the native libraries loaded in this
        process, to share with other sessions. If null, the session makes
        one of its own.
        @return The session, or the error if an extension failed to load or
        a generator could not be added.
    */
    static Expected<Session>
    create(
        Config config,
        std::shared_ptr<ExtensionRegistry::LibraryRegistry> libraries =
            nullptr);

    /** Destructor.
    */
    ~Session();

    /** Move constructor.

        The references the source returned from @ref log, @ref generators
        and @ref extensions refer to the new session.
    */
    Session(Session&&) noexcept;

    /** Move assignment.
    */
    Session& operator=(Session&&) noexcept;

    Session(Session const&) = delete;
    Session& operator=(Session const&) = delete;

    /** Return the configuration of the run.

        @return The configuration the session was created with. It lives as
        long as the session.
    */
    Config const&
    config() const noexcept;

    /** Return the generators of the run.

        @return The registry of every generator, whatever defined it.
    */
    GeneratorRegistry&
    generators() noexcept;

    /// @copydoc generators()
    GeneratorRegistry const&
    generators() const noexcept;

    /** Return the extension registry of the run.

        @return The registry that loads the extensions and owns the
        transform pipeline.
    */
    ExtensionRegistry&
    extensions() noexcept;

    /// @copydoc extensions()
    ExtensionRegistry const&
    extensions() const noexcept;

    /** Return the reporter of the run.

        @return The reporter that prints and counts the messages of the
        run.
    */
    report::Reporter&
    log() const noexcept;

    /** Apply the pipeline of transforms to a corpus.

        Forwards to @ref ExtensionRegistry::applyTransforms with the
        configuration of the session. The context of each transform
        carries the reporter of the session.

        @param corpus The corpus to transform. It must outlive the
        session.
        @return The error, if any occurred, naming the transform it came
        from.
    */
    Expected<void>
    applyTransforms(Corpus& corpus) const;

    /** Build the documentation of a corpus with a generator.

        Calls @ref Generator::build with the configuration of the session,
        from which the generator reads its options with
        @ref getGeneratorParams. It passes the generator the reporter of
        the session, the same one @ref applyTransforms gives the
        transforms.

        @param generator The generator to run, usually one of
        @ref generators.
        @param corpus The symbols to emit.
        @return The error, if any occurred.
    */
    Expected<void>
    build(Generator const& generator, Corpus const& corpus) const;

private:
    struct Impl;

    explicit Session(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // mrdocs

#endif // MRDOCS_API_SESSION_HPP
