//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//

#include "Diff.hpp"

namespace mrdocs::example {

char const*
semverLabel(SemverImpact v)
{
    switch (v)
    {
        case SemverImpact::Patch: return "patch";
        case SemverImpact::Minor: return "minor";
        case SemverImpact::Major: return "major";
    }
    return "?";
}

namespace {

// A breaking-change audit cares about the library's reachable
// surface: public top-level symbols. Private members, overload
// buckets, and namespaces are not part of the contract.
bool
isPublicTopLevel(Symbol const& s)
{
    bool const isPublic =
        s.Access == AccessKind::Public ||
        s.Access == AccessKind::None;
    return isPublic && !s.isNamespace() && !s.isOverloads();
}

// True when a function declaration is `noexcept` as written by the
// user. An implicit `noexcept` (inherited from defaults, e.g. a
// destructor) is not a contract the caller can rely on changing.
bool
isExplicitNoexcept(FunctionSymbol const& F)
{
    return !F.Noexcept.Implicit
        && F.Noexcept.Kind == NoexceptKind::True;
}

// tag::compare-functions[]
// Compare two function symbols field by field. Each entry in the
// returned vector is one aspect that differs. The function never
// constructs a signature string for comparison; equality of typed
// fields is the source of truth.
std::vector<std::string>
compareFunctions(FunctionSymbol const& a, FunctionSymbol const& b)
{
    std::vector<std::string> reasons;

    if (a.ReturnType != b.ReturnType)
    {
        reasons.emplace_back("return type");
    }

    if (a.Params.size() != b.Params.size())
    {
        reasons.emplace_back("parameter count");
    }
    else
    {
        for (std::size_t i = 0; i < a.Params.size(); ++i)
        {
            if (a.Params[i].Type != b.Params[i].Type)
            {
                reasons.emplace_back("parameter types");
                break;
            }
        }
    }

    if (a.IsConst       != b.IsConst    ||
        a.IsVolatile    != b.IsVolatile ||
        a.RefQualifier  != b.RefQualifier)
    {
        reasons.emplace_back("cv/ref qualifiers");
    }

    if (isExplicitNoexcept(a) != isExplicitNoexcept(b))
    {
        reasons.emplace_back("noexcept");
    }

    return reasons;
}
// end::compare-functions[]

} // (anon)

// tag::diff[]
DiffResult
diff(Corpus const& v1, Corpus const& v2)
{
    DiffResult result;

    // For every public symbol in v1, see what happened in v2.
    for (Symbol const& a : v1)
    {
        if (!isPublicTopLevel(a))
        {
            continue;
        }
        std::string name = v1.qualifiedName(a);
        auto found = v2.lookup(name);
        if (!found)
        {
            result.removed.emplace_back(std::move(name), &a);
            continue;
        }
        Symbol const& b = *found;

        if (a.Kind != b.Kind)
        {
            // A function turned into a record, or similar.
            // Treat it as removed + added rather than changed.
            result.added.emplace_back(name, &b);
            result.removed.emplace_back(std::move(name), &a);
            continue;
        }

        if (a.isFunction())
        {
            auto reasons = compareFunctions(a.asFunction(), b.asFunction());
            if (!reasons.empty())
            {
                result.changed.push_back(
                    {std::move(name), &a, &b, std::move(reasons)});
            }
        }
    }

    // Anything public in v2 that wasn't in v1 is a new addition.
    for (Symbol const& b : v2)
    {
        if (!isPublicTopLevel(b))
        {
            continue;
        }
        std::string name = v2.qualifiedName(b);
        if (!v1.lookup(name))
        {
            result.added.emplace_back(std::move(name), &b);
        }
    }

    if (!result.removed.empty() || !result.changed.empty())
    {
        result.impact = SemverImpact::Major;
    }
    else if (!result.added.empty())
    {
        result.impact = SemverImpact::Minor;
    }
    return result;
}
// end::diff[]

} // namespace mrdocs::example
