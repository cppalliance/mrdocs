//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "FilteredSymbolFinalizer.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/InputFiles.hpp>
#include <mrdocs/Support/Report.hpp>

namespace mrdocs {

namespace {

bool
isFilteredProjectSymbol(Symbol const& I, Config const& config)
{
    MRDOCS_CHECK_OR(I.Extraction == ExtractionMode::Dependency, false);
    Optional<Location> const loc = getPrimaryLocation(I);
    MRDOCS_CHECK_OR(loc, false);
    return isInputFile(config, loc->FullPath);
}

} // (anon)

void
FilteredSymbolFinalizer::
reportIfFiltered(Symbol const& referrer, Polymorphic<Type> const& type)
{
    MRDOCS_CHECK_OR(type);
    MRDOCS_ASSERT(!type.valueless_after_move());

    // The specifiers around the type say nothing about what is named:
    // `detail::token const&` names the same symbol as `detail::token`.
    Polymorphic<Type> const& named = innermostType(type);
    MRDOCS_CHECK_OR(named);
    MRDOCS_CHECK_OR(named->isNamed());
    MRDOCS_CHECK_OR(named->asNamed().Name);
    reportIfFiltered(referrer, *named->asNamed().Name);
}

void
FilteredSymbolFinalizer::
reportIfFiltered(Symbol const& referrer, Name const& name)
{
    bool throughSpecialization = false;
    for (Name const* current = &name;
         current;
         current = current->Prefix ? &**current->Prefix : nullptr)
    {
        MRDOCS_CHECK_OR_CONTINUE(current->Kind == NameKind::Specialization);
        throughSpecialization = true;
        for (Polymorphic<TArg> const& arg :
                 dynamic_cast<SpecializationName const&>(*current).TemplateArgs)
        {
            MRDOCS_CHECK_OR_CONTINUE(arg);
            MRDOCS_CHECK_OR_CONTINUE(arg->Kind == TArgKind::Type);
            reportIfFiltered(referrer, arg->asType().Type);
        }
    }

    MRDOCS_CHECK_OR(!throughSpecialization);

    SymbolID const id = name.id;
    MRDOCS_CHECK_OR(id != SymbolID::invalid);

    Symbol const* target = corpus_.find(id);
    MRDOCS_CHECK_OR(target);
    MRDOCS_CHECK_OR(isFilteredProjectSymbol(*target, config_));

    Optional<Location> const loc = getPrimaryLocation(referrer);
    MRDOCS_CHECK_OR(loc);

    report::Level const level = config_.warnAsError
        ? report::Level::error
        : report::Level::warn;
    report::log(
        level,
        "{}:{}: '{}' names '{}', which the filters exclude",
        loc->ShortPath,
        loc->LineNumber,
        corpus_.qualifiedName(referrer),
        corpus_.qualifiedName(*target));
}

void
FilteredSymbolFinalizer::
build()
{
    for (auto& I : corpus_.info_)
    {
        MRDOCS_ASSERT(I);
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        visit(*I, [&]<typename SymbolTy>(SymbolTy const& sym)
        {
            if constexpr (SymbolTy::isFunction())
            {
                reportIfFiltered(sym, sym.ReturnType);
                for (Param const& param : sym.Params)
                {
                    reportIfFiltered(sym, param.Type);
                }
            }
            else if constexpr (SymbolTy::isRecord())
            {
                for (BaseInfo const& base : sym.Bases)
                {
                    MRDOCS_CHECK_OR_CONTINUE(base.Access == AccessKind::Public);
                    reportIfFiltered(sym, base.Type);
                }
            }
            else if constexpr (SymbolTy::isTypedef() || SymbolTy::isVariable())
            {
                reportIfFiltered(sym, sym.Type);
            }
        });
    }
}

} // mrdocs
