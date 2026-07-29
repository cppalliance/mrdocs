//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//

#ifndef MRDOCS_EXAMPLE_BREAKING_CHANGES_DIFF_HPP
#define MRDOCS_EXAMPLE_BREAKING_CHANGES_DIFF_HPP

#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <string>
#include <utility>
#include <vector>

namespace mrdocs::example {

// Removed or changed symbols escalate the verdict from `minor` to
// `major`. A run with no removed or changed symbols and at least
// one addition lands at `minor`; everything else is `patch`.
enum class SemverImpact
{
    Patch,
    Minor,
    Major,
};

char const*
semverLabel(SemverImpact v);

// One function changed between two corpora. `reasons` lists which
// of the typed fields differ, as human-readable strings (e.g.
// "return type", "noexcept"). Names come from comparing the
// structured data; the diff never round-trips through a signature
// string.
struct ChangedFunction
{
    std::string qualifiedName;
    Symbol const* before;
    Symbol const* after;
    std::vector<std::string> reasons;
};

// The diff between two corpora. Pointers refer into the corpora
// that produced them and must outlive the result.
struct DiffResult
{
    std::vector<std::pair<std::string, Symbol const*>> added;
    std::vector<std::pair<std::string, Symbol const*>> removed;
    std::vector<ChangedFunction> changed;
    SemverImpact impact = SemverImpact::Patch;
};

DiffResult
diff(Corpus const& v1, Corpus const& v2);

} // namespace mrdocs::example

#endif
