//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// MrDocs example: breaking-change detector.
//
// Loads two corpora described by two `mrdocs.yml` configurations
// (one baseline, one candidate), constructs a custom Generator
// subclass that owns the report's output format, adds it to the
// generators of a Session, looks it back up by id, and asks it to render
// the diff to stdout.
//
// Removed public symbols and function-signature changes are
// flagged as major; added public symbols as minor; doc-only
// changes (or no changes) as patch.
//
// Links the mrdocs-core library and uses only public headers
// under include/mrdocs/.
//

#include "BreakingChangesGenerator.hpp"
#include "Corpus.hpp"
#include <mrdocs/Generator.hpp>
#include <mrdocs/Session.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Support/Concurrency/ThreadPool.hpp>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <print>
#include <string>

int main(int argc, char const** argv)
{
    using namespace mrdocs;

    if (argc < 3)
    {
        std::println(stderr,
            "usage: {} <v1/mrdocs.yml> <v2/mrdocs.yml>",
            argv[0] ? argv[0] : "breaking-changes");
        return 2;
    }

    // A default-constructed ReferenceDirectories points at MrDocs's installed
    // root (linking mrdocs-core bakes it in), so the built-in resources resolve
    // with no configuration. Any arguments after the two configs are option
    // overrides passed straight through: an installed run supplies none, while
    // the in-tree test forwards the built-in directory flags.
    ReferenceDirectories dirs;
    char const** overrides = argv + 3;

    // tag::install-and-run[]
    // The session owns the reporter of the run, so it comes first: both
    // corpora report through session.log(), which prints errors only
    Expected<Session> created = Session::create(Config());
    if (!created)
    {
        std::println(stderr, "Session::create: {}", created.error().reason());
        return 1;
    }
    Session& session = *created;
    session.log().setMinimumLevel(report::Level::error);
    auto v1Corpus = example::loadCorpusFromConfig(
        argv[1], dirs, overrides, session.log());
    if (!v1Corpus)
    {
        std::println(stderr, "v1: {}", v1Corpus.error().reason());
        return 1;
    }
    auto v2Corpus = example::loadCorpusFromConfig(
        argv[2], dirs, overrides, session.log());
    if (!v2Corpus)
    {
        std::println(stderr, "v2: {}", v2Corpus.error().reason());
        return 1;
    }

    auto installed = session.generators().install(
        std::make_unique<example::BreakingChangesGenerator>(*v1Corpus));
    if (!installed)
    {
        std::println(stderr,
            "install: {}", installed.error().reason());
        return 1;
    }

    Generator const* gen =
        session.generators().find("breaking-changes");
    if (!gen)
    {
        std::println(stderr,
            "find: breaking-changes not installed");
        return 1;
    }

    auto wrote = session.build(*gen, *v2Corpus);
    if (!wrote)
    {
        std::println(stderr,
            "Generator::build: {}", wrote.error().reason());
        return 1;
    }
    // end::install-and-run[]

    return 0;
}
