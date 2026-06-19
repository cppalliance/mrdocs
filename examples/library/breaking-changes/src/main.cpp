//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// MrDocs example: breaking-change detector.
//
// Loads two corpora described by two `mrdocs.yml` configurations
// (one baseline, one candidate), constructs a custom Generator
// subclass that owns the report's output format, installs it into
// the process-global registry, looks it back up by id, and asks
// it to render the diff to stdout.
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
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/ThreadPool.hpp>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char const** argv)
{
    using namespace mrdocs;

    if (argc < 3)
    {
        std::fprintf(stderr,
            "usage: %s <v1/mrdocs.yml> <v2/mrdocs.yml>\n",
            argv[0] ? argv[0] : "breaking-changes");
        return 2;
    }

    report::setMinimumLevel(report::Level::error);

    ReferenceDirectories dirs;
    if (char const* root = std::getenv("MRDOCS_ROOT"))
    {
        dirs.mrdocsRoot = root;
    }
    ThreadPool threadPool(/*concurrency=*/1);

    auto v1Corpus = example::loadCorpusFromConfig(
        argv[1], dirs, threadPool);
    if (!v1Corpus)
    {
        std::fprintf(stderr, "v1: %s\n",
            v1Corpus.error().reason().c_str());
        return 1;
    }
    auto v2Corpus = example::loadCorpusFromConfig(
        argv[2], dirs, threadPool);
    if (!v2Corpus)
    {
        std::fprintf(stderr, "v2: %s\n",
            v2Corpus.error().reason().c_str());
        return 1;
    }

    // tag::install-and-run[]
    auto installed = installGenerator(
        std::make_unique<example::BreakingChangesGenerator>(**v1Corpus));
    if (!installed)
    {
        std::fprintf(stderr,
            "installGenerator: %s\n",
            installed.error().reason().c_str());
        return 1;
    }

    Generator const* gen = findGenerator("breaking-changes");
    if (!gen)
    {
        std::fprintf(stderr,
            "findGenerator: breaking-changes not installed\n");
        return 1;
    }

    auto wrote = gen->build(**v2Corpus);
    if (!wrote)
    {
        std::fprintf(stderr,
            "Generator::build: %s\n",
            wrote.error().reason().c_str());
        return 1;
    }
    // end::install-and-run[]

    return 0;
}
