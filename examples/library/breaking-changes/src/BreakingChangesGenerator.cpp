//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#include "BreakingChangesGenerator.hpp"
#include "Diff.hpp"

#include <algorithm>
#include <iostream>

namespace mrdocs::example {

std::string_view
BreakingChangesGenerator::
id() const noexcept
{
    return "breaking-changes";
}

std::string_view
BreakingChangesGenerator::
displayName() const noexcept
{
    return "Breaking-change report";
}

std::string_view
BreakingChangesGenerator::
fileExtension() const noexcept
{
    return "txt";
}

namespace {

char const*
kindName(SymbolKind k)
{
    switch (k)
    {
        case SymbolKind::Namespace:     return "namespace";
        case SymbolKind::Record:        return "record";
        case SymbolKind::Function:      return "function";
        case SymbolKind::Enum:          return "enum";
        case SymbolKind::EnumConstant:  return "enum constant";
        case SymbolKind::Typedef:       return "typedef";
        case SymbolKind::Variable:      return "variable";
        case SymbolKind::Guide:         return "guide";
        case SymbolKind::NamespaceAlias:return "namespace alias";
        case SymbolKind::Using:         return "using";
        case SymbolKind::Concept:       return "concept";
        case SymbolKind::Overloads:     return "overloads";
        default:                        return "symbol";
    }
}

// tag::write-report[]
void
writeReport(std::ostream& os, DiffResult const& d)
{
    auto added   = d.added;
    auto removed = d.removed;
    auto changed = d.changed;
    std::ranges::sort(added,   {}, &decltype(added)::value_type::first);
    std::ranges::sort(removed, {}, &decltype(removed)::value_type::first);
    std::ranges::sort(changed, {}, &ChangedFunction::qualifiedName);

    os << "== Breaking-change report ==\n\n";

    os << "Added (" << added.size() << "):\n";
    for (auto const& [name, sym] : added)
    {
        os << "  + [" << kindName(sym->Kind) << "] " << name << '\n';
    }

    os << "\nRemoved (" << removed.size() << "):\n";
    for (auto const& [name, sym] : removed)
    {
        os << "  - [" << kindName(sym->Kind) << "] " << name << '\n';
    }

    os << "\nChanged (" << changed.size() << "):\n";
    for (auto const& c : changed)
    {
        os << "  * [" << kindName(c.after->Kind) << "] "
           << c.qualifiedName << '\n';
        for (auto const& r : c.reasons)
        {
            os << "      - " << r << " differs\n";
        }
    }

    os << "\nSuggested version bump: "
       << semverLabel(d.impact) << '\n';
}
// end::write-report[]

} // (anon)

// tag::build[]
// build() does whatever the generator needs. This one is not a
// file-producing documentation format: it diffs the baseline against the
// candidate corpus and prints the report to stdout. A generator is free
// to bring its output to the screen instead of writing files.
Expected<void>
BreakingChangesGenerator::
build(Corpus const& current) const
{
    writeReport(std::cout, diff(*baseline_, current));
    return {};
}
// end::build[]

} // namespace mrdocs::example
