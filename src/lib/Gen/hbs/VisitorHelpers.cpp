//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "VisitorHelpers.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs::hbs {

bool
shouldGenerate(Info const& I, Config const& config)
{
    if (I.isEnumConstant() && !config->showEnumConstants)
    {
        return false;
    }
    if (I.Extraction == ExtractionMode::Dependency)
    {
        return false;
    }
    if (I.Extraction == ExtractionMode::ImplementationDefined)
    {
        // We don't generate pages for implementation-defined symbols.
        // We do generate pages for see-below symbols.
        // See the requirements in ConfigOptions.json.
        return false;
    }
    if (!config->showNamespaces && I.isNamespace())
    {
        return false;
    }
    return true;
}

namespace {

// Resolve a typedef to its underlying Info type
Info const*
resolveTypedef(Corpus const& c, Info const& I)
{
    if (I.isTypedef())
    {
        auto const& TI = I.asTypedef();
        MRDOCS_CHECK_OR(TI.Type, &I);
        MRDOCS_ASSERT(!TI.Type.valueless_after_move());
        TypeInfo const& T = *TI.Type;
        MRDOCS_CHECK_OR(T.Kind == TypeKind::Named, &I);
        auto const& NT = dynamic_cast<NamedTypeInfo const&>(T);
        MRDOCS_CHECK_OR(NT.Name, &I);
        Info const* resolved = c.find(NT.Name->id);
        MRDOCS_CHECK_OR(resolved, &I);
        if (resolved->Kind == InfoKind::Typedef)
        {
            return resolveTypedef(c, *resolved);
        }
        return resolved;
    }
    return &I;
}

/* Look for an equivalent symbol in the parent Info
 */
Info const*
findPrimarySiblingWithUrl(Corpus const& c, Info const& I, Info const& parent)
{
    // Look for the primary sibling in the parent scope
    MRDOCS_CHECK_OR(parent, nullptr);
    return visit(parent, [&]<typename InfoTy>(InfoTy const& U)
        -> Info const*
    {
        if constexpr (InfoParent<InfoTy>)
        {
            namespace stdv = std::ranges::views;
            auto sameNameSiblings =
                allMembers(U) |
                stdv::transform([&](SymbolID const& siblingID)
                {
                    return c.find(siblingID);
                }) |
                stdv::filter([&](Info const* sibling)
                {
                    return sibling && sibling->Name == I.Name;
                });
            for (Info const* sibling: sameNameSiblings)
            {
                if (!sibling ||
                    !shouldGenerate(*sibling, c.config))
                {
                    continue;
                }
                bool const isPrimarySibling = visit(*sibling, [&](auto const& V)
                {
                    if constexpr (requires { V.Template; })
                    {
                        Optional<TemplateInfo> const& Template = V.Template;
                        MRDOCS_CHECK_OR(Template, false);
                        return !Template->Params.empty() && Template->Args.empty();
                    }
                    return false;
                });
                if (isPrimarySibling)
                {
                    return sibling;
                }
            }
        }
        return nullptr;
    });
}

Info const*
findPrimarySiblingWithUrl(Corpus const& c, Info const& I);

/* Find the parent and look for equivalent symbol in the parent

   This function will look for the parent and, if the parent
   should be generated but its member should not, we look
   for an equivalent symbol in the parent.

   On the other hand, if the parent should not be generated,
   we look for a symbol equivalent to the parent, and then look
   for an equivalent symbol in the parent.
 */
Info const*
findDirectPrimarySiblingWithUrl(Corpus const& c, Info const& I)
{
    // If the parent is a scope, look for a primary sibling
    // in the parent scope for which we want to generate the URL
    Info const* parent = c.find(I.Parent);
    MRDOCS_CHECK_OR(parent, nullptr);
    if (!shouldGenerate(*parent, c.config))
    {
        parent = findPrimarySiblingWithUrl(c, *parent);
        MRDOCS_CHECK_OR(parent, nullptr);
    }
    return findPrimarySiblingWithUrl(c, I, *parent);
}

/* Resolve typedefs and look for equivalent symbol

    This function will resolve typedefs and look for an equivalent
    symbol in the parent scope for which we want to generate the URL.
 */
Info const*
findResolvedPrimarySiblingWithUrl(Corpus const& c, Info const& I)
{
    // Check if this info is a specialization or a typedef to
    // a specialization, otherwise there's nothing to resolve
    bool const isSpecialization = visit(I, [&]<typename InfoTy>(InfoTy const& U)
    {
        // The symbol is a specialization
        if constexpr (requires { U.Template; })
        {
            Optional<TemplateInfo> const& Template = U.Template;
            if (Template &&
                !Template->Args.empty())
            {
                return true;
            }
        }
        // The symbol is a typedef to a specialization
        if constexpr (std::same_as<InfoTy, TypedefInfo>)
        {
            auto const& TOpt = U.Type;
            MRDOCS_CHECK_OR(!TOpt.valueless_after_move(), false);
            TypeInfo const& T = *TOpt;
            MRDOCS_CHECK_OR(T.Kind == TypeKind::Named, false);
            auto const& NT = dynamic_cast<NamedTypeInfo const&>(T);
            MRDOCS_CHECK_OR(NT.Name, false);
            MRDOCS_CHECK_OR(NT.Name->Kind == NameKind::Specialization, false);
            return true;
        }
        return false;
    });
    MRDOCS_CHECK_OR(isSpecialization, nullptr);

    // Find the parent scope containing the primary sibling
    // for which we want to generate the URL
    Info const* parent = c.find(I.Parent);
    MRDOCS_CHECK_OR(parent, nullptr);

    // If the parent is a typedef, resolve it
    // so we can iterate the members of this scope.
    // We can't find siblings in a typedef because
    // it's not a scope.
    if (parent->Kind == InfoKind::Typedef)
    {
        parent = resolveTypedef(c, *parent);
        MRDOCS_CHECK_OR(parent, nullptr);
    }

    // If the resolved parent is also a specialization or
    // a dependency for which there's no URL, we attempt to
    // find the primary sibling for the parent so we take
    // the URL from it.
    if (!shouldGenerate(*parent, c.config))
    {
        parent = findPrimarySiblingWithUrl(c, *parent);
        MRDOCS_CHECK_OR(parent, nullptr);
    }

    return findPrimarySiblingWithUrl(c, I, *parent);
}

Info const*
findPrimarySiblingWithUrl(Corpus const& c, Info const& I)
{
    if (Info const* primary = findDirectPrimarySiblingWithUrl(c, I))
    {
        return primary;
    }
    return findResolvedPrimarySiblingWithUrl(c, I);
}

/* Find a parent symbol whose URL we can use for I

    Unlike findPrimarySiblingWithUrl, which attempts
    to find an equivalent symbol. This function will
    look for a parent symbol whose URL we can use
    for the specified Info and just use the parent.

    However, namespaces are not considered valid parents
    for generating URLs because it would be misleading
    to generate a URL for a namespace when the user
    is looking for a URL for a symbol.

 */
Info const*
findParentWithUrl(Corpus const& c, Info const& I)
{
    Info const* parent = c.find(I.Parent);
    MRDOCS_CHECK_OR(parent, nullptr);
    MRDOCS_CHECK_OR(!parent->isNamespace(), nullptr);
    if (shouldGenerate(*parent, c.config))
    {
        return parent;
    }
    parent = findPrimarySiblingWithUrl(c, *parent);
    MRDOCS_CHECK_OR(parent, nullptr);
    MRDOCS_CHECK_OR(!parent->isNamespace(), nullptr);
    return parent;
}
}

Info const*
findAlternativeURLInfo(Corpus const& c, Info const& I)
{
    if (Info const* primary = findPrimarySiblingWithUrl(c, I))
    {
        return primary;
    }
    return findParentWithUrl(c, I);
}

} // clang::mrdocs::hbs
