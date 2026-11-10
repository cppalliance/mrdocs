//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Generator.hpp>
#include <mrdocs/Session.hpp>
#include <utility>

namespace mrdocs {

struct Session::Impl
{
    Config config;
    report::Reporter reporter;
    ExtensionRegistry extensions;

    Impl(
        Config config_,
        std::shared_ptr<ExtensionRegistry::LibraryRegistry> libraries)
        : config(std::move(config_))
        , reporter(static_cast<report::Level>(config.logLevel))
        , extensions(std::move(libraries))
    {
        reporter.setWarnAsError(config.warnAsError);
    }
};

Session::Session(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}

Session::~Session() = default;

Session::Session(Session&&) noexcept = default;

Session& Session::operator=(Session&&) noexcept = default;

Expected<Session>
Session::create(
    Config config,
    std::shared_ptr<ExtensionRegistry::LibraryRegistry> libraries)
{
    Session session(std::make_unique<Impl>(
        std::move(config), std::move(libraries)));
    Impl& impl = *session.impl_;
    // Scripts and native plugins are loaded together, in the order of their
    // names. A plugin that cannot install what it provides fails the run,
    // while an addon generator directory whose id is already taken is
    // skipped. The extensions therefore come before the discovery of the
    // generators of the addons.
    MRDOCS_TRY(impl.extensions.loadExtensions(impl.config, impl.reporter));
    MRDOCS_TRY(hbs::discoverDataDrivenGenerators(
        impl.config, impl.extensions.generators()));
    return session;
}

Config const&
Session::config() const noexcept
{
    return impl_->config;
}

GeneratorRegistry&
Session::generators() noexcept
{
    return impl_->extensions.generators();
}

GeneratorRegistry const&
Session::generators() const noexcept
{
    return impl_->extensions.generators();
}

report::Reporter&
Session::log() const noexcept
{
    return impl_->reporter;
}

ExtensionRegistry&
Session::extensions() noexcept
{
    return impl_->extensions;
}

ExtensionRegistry const&
Session::extensions() const noexcept
{
    return impl_->extensions;
}

Expected<void>
Session::applyTransforms(Corpus& corpus) const
{
    return impl_->extensions.applyTransforms(
        corpus, impl_->config, impl_->reporter);
}

Expected<void>
Session::build(Generator const& generator, Corpus const& corpus) const
{
    return generator.build(corpus, impl_->config, impl_->reporter);
}

} // mrdocs
