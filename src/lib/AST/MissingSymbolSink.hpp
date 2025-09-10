//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_MISSING_SYMBOL_SINK_HPP
#define MRDOCS_LIB_AST_MISSING_SYMBOL_SINK_HPP

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/SourceManager.h"
#include "lib/ConfigImpl.hpp"
#include "lib/Support/ExecutionContext.hpp"
#include "lib/Support/Report.hpp"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSet.h"
#include <mrdocs/Platform.hpp>
#include <format>
#include <mutex>
#include <set>
#include <string>

namespace clang {
namespace mrdocs {

struct FrozenDiag {
    clang::DiagnosticsEngine::Level level{};
    unsigned id{};
    std::string msg;
    std::string file;
    unsigned line = 0, col = 0;
    std::string optFlag;
};

/* This class stores missing symbols in a TU that missing includes

   When clang encounters an unknown type or namespace, it emits an error.
   If the corresponding MrDocs option is enabled, we intercept these
   errors, extract the missing symbol names, and store them.

   After parsing the TU, if the missing symbol names are not ambiguous
   (i.e., a symbol that could be a namespace or a type), we generate
    a shim header that declares these symbols as stubs and reparse the TU
    with the shim included.

    This allows us to extract documentation from TUs with dependencies
    That wouldn't be reasonable to always include, such as compiling
    LLVM in a new CI environment, only to have the header files available
    and then extract documentation.

    The process of creating stubs is not perfect. It's based on
    heuristics and likely to fail for complex cases. However,
    it works well enough for simple cases and is opt-in. For
    other cases, the user can provide the shims via the
    missing-include-shims option.
 */
class MissingSymbolSink {
    // AFREITAS: This should be a tree structure to avoid ambiguities
    // such as a symbol being both a type and a namespace.
    // Whenever a type is added, we should check what kind of context
    // it could be in considering all instances, and if it's ambiguous,
    // we should fail.
    std::set<std::string> Types;
    std::set<std::string> Namespaces;
    std::vector<FrozenDiag> deferred;
    std::size_t prevSize = 0;
    mutable std::mutex Mu;

public:
    void
    addType(llvm::StringRef s)
    {
        std::lock_guard<std::mutex> L(Mu);
        Types.insert(s.str());
    }
    void
    addNamespace(llvm::StringRef s)
    {
        std::lock_guard<std::mutex> L(Mu);
        Namespaces.insert(s.str());
    }

    std::size_t
    numTypes() const
    {
        std::lock_guard<std::mutex> L(Mu);
        return Types.size();
    }

    std::size_t
    numNamespaces() const
    {
        std::lock_guard<std::mutex> L(Mu);
        return Namespaces.size();
    }

    std::size_t
    numSymbols() const
    {
        return Types.size() + Namespaces.size();
    }

    void
    deferDiagnostic(
        clang::DiagnosticsEngine::Level L,
        clang::Diagnostic const &Info)
    {
        llvm::SmallString<256> buf;
        Info.FormatDiagnostic(buf);

        std::string file;
        unsigned line = 0, col = 0;
        if (Info.getLocation().isValid())
        {
            auto P = Info.getSourceManager().getPresumedLoc(Info.getLocation());
            if (P.isValid())
            {
                file = P.getFilename();
                line = P.getLine();
                col = P.getColumn();
            }
        }
        StringRef flag = Info.getFlagValue();

        FrozenDiag d;
        d.level = L;
        d.id = Info.getID();
        d.msg = std::string(buf);
        d.file = std::move(file);
        d.line = line;
        d.col = col;
        if (!flag.empty())
        {
            d.optFlag = flag;
        }

        std::lock_guard<std::mutex> lock(Mu);
        deferred.emplace_back(std::move(d));
    }

    std::vector<FrozenDiag>
    consumeDeferred()
    {
        std::lock_guard<std::mutex> lock(Mu);
        auto out = std::move(deferred);
        deferred.clear();
        return out;
    }

    void
    setStartParsing()
    {
        std::lock_guard<std::mutex> L(Mu);
        prevSize = numSymbols();
    }

    bool
    symbolsAdded() const
    {
        std::lock_guard<std::mutex> L(Mu);
        return numSymbols() > prevSize;
    }

    std::string
    buildShim()
    {
        MissingSymbolSink const &S = *this;
        std::string H;
        // __mrdocs_shims/virtual_diagnostics_shim
        H += "#pragma clang system_header\n"
             "#ifndef MRDOCS_SHIMS_VIRTUAL_DIAGNOSTICS_DRIVEN_SHIM\n"
             "#define MRDOCS_SHIMS_VIRTUAL_DIAGNOSTICS_DRIVEN_SHIM\n"
             "#ifdef __cplusplus\n";
        for (auto const &E: S.Namespaces)
        {
            std::format_to(std::back_inserter(H), "namespace {} {{}}\n", E);
        }
        for (auto const &E: S.Types)
        {
            auto qualifiersRange = llvm::split(E, "::");
            auto qualifiers = llvm::to_vector(qualifiersRange);
            auto n = qualifiers.size();
            if (n > 0)
            {
                // All but last are namespaces
                for (std::size_t i = 0; i + 1 < n; ++i)
                {
                    std::format_to(
                        std::back_inserter(H),
                        "namespace {} {{ ",
                        qualifiers[i]);
                }
                // Last is class
                std::format_to(
                    std::back_inserter(H),
                    "class {}; ",
                    qualifiers[n - 1]);
                // Close namespaces
                for (std::size_t i = 0; i + 1 < n; ++i)
                {
                    H += "}";
                }
                H += '\n';
            }
        }
        H += "#endif\n"
             "#endif\n";
        return H;
    }
};

class CollectingDiagConsumer : public clang::DiagnosticConsumer {
    MissingSymbolSink &Sink_;
    std::unique_ptr<clang::DiagnosticConsumer> Downstream_;
    clang::DiagnosticsEngine &DE_;
    bool Replaying_ = false;

    static std::string
    firstQuoted(llvm::StringRef s)
    {
        std::size_t l = s.find('\''),
                    r = (l == llvm::StringRef::npos) ? l : s.find('\'', l + 1);
        return (l != llvm::StringRef::npos && r != llvm::StringRef::npos
                && r > l + 1) ?
                   s.substr(l + 1, r - l - 1).str() :
                   std::string{};
    }

    static std::pair<std::string_view, std::string_view>
    firstAndSecondQuoted(llvm::StringRef s)
    {
        std::size_t l1 = s.find('\'');
        if (l1 == llvm::StringRef::npos)
        {
            return { std::string_view{}, std::string_view{} };
        }
        std::size_t r1 = s.find('\'', l1 + 1);
        if (r1 == llvm::StringRef::npos)
        {
            return { std::string_view{}, std::string_view{} };
        }
        std::size_t l2 = s.find('\'', r1 + 1);
        if (l2 == llvm::StringRef::npos)
        {
            return { s.substr(l1 + 1, r1 - l1 - 1), std::string_view{} };
        }
        std::size_t r2 = s.find('\'', l2 + 1);
        if (r2 == llvm::StringRef::npos)
        {
            return { s.substr(l1 + 1, r1 - l1 - 1), std::string_view{} };
        }
        return { s.substr(l1 + 1, r1 - l1 - 1), s.substr(l2 + 1, r2 - l2 - 1) };
    }
public:
    CollectingDiagConsumer(
        MissingSymbolSink &S,
        std::unique_ptr<clang::DiagnosticConsumer> Prev,
        clang::DiagnosticsEngine &DE)
        : Sink_(S)
        , Downstream_(std::move(Prev))
        , DE_(DE)
    {}

    void
    HandleDiagnostic(
        clang::DiagnosticsEngine::Level L,
        clang::Diagnostic const &Info) override
    {
        if (L <= clang::DiagnosticsEngine::Warning)
        {
            return;
        }

        if (Replaying_)
        {
            if (Downstream_)
            {
                Downstream_->HandleDiagnostic(L, Info);
            }
            DiagnosticConsumer::HandleDiagnostic(L, Info);
            return; // let the engine count this; don't capture again
        }


        switch (Info.getID())
        {
        case clang::diag::err_unknown_typename:
        case clang::diag::err_incomplete_nested_name_spec:
        {
            llvm::SmallString<256> Msg;
            Info.FormatDiagnostic(Msg);
            auto name = firstQuoted(Msg);
            if (!name.empty() && name != "std")
            {
                Sink_.addType(name);
                Sink_.deferDiagnostic(L, Info);
                return;
            }
            break;
        }
        case clang::diag::err_undeclared_use:
        case clang::diag::err_undeclared_use_suggest:
        case clang::diag::err_undeclared_var_use:
        case clang::diag::err_undeclared_var_use_suggest:
        {
            llvm::SmallString<256> Msg;
            Info.FormatDiagnostic(Msg);
            auto name = firstQuoted(Msg);
            if (!name.empty() && name != "std")
            {
                Sink_.addNamespace(name);
                Sink_.deferDiagnostic(L, Info);
                return;
            }
            break;
        }
        case clang::diag::err_typename_nested_not_found:
        case clang::diag::err_unknown_nested_typename_suggest:
        case clang::diag::err_no_member:
        case clang::diag::err_no_member_overloaded_arrow:
        case clang::diag::err_no_member_suggest:
        {
            // no type named %0 in %1
            llvm::SmallString<256> Msg;
            Info.FormatDiagnostic(Msg);
            auto [name, context] = firstAndSecondQuoted(Msg);
            if (!context.empty())
            {
                Sink_.addNamespace(context);
                if (!name.empty() && name != "std")
                {
                    std::string qualified = std::format("{}::{}", context, name);
                    Sink_.addType(qualified);
                    Sink_.deferDiagnostic(L, Info);
                    return;
                }
            } else
            {
                if (!name.empty() && name != "std")
                {
                    Sink_.addType(name);
                    Sink_.deferDiagnostic(L, Info);
                    return;
                }
            }
            break;
        }
        default:
            break;
        }

        llvm::SmallString<256> Msg;
        Info.FormatDiagnostic(Msg);
        if (Downstream_)
        {
            Downstream_->HandleDiagnostic(L, Info);
        }
        DiagnosticConsumer::HandleDiagnostic(L, Info);
    }

    void
    BeginSourceFile(clang::LangOptions const &LO, clang::Preprocessor const *PP)
        override
    {
        if (Downstream_)
        {
            Downstream_->BeginSourceFile(LO, PP);
        }
    }

    void
    EndSourceFile() override
    {
        if (!Downstream_)
        {
            return;
        }

        auto frozen = Sink_.consumeDeferred();
        if (!Sink_.symbolsAdded() &&
            !frozen.empty())
        {
            // we deferred some errors but no new symbols
            Replaying_ = true;
            unsigned emittedErrors = 0;

            for (auto const &d: frozen)
            {
                std::string prefix;
                if (!d.file.empty())
                {
                    llvm::raw_string_ostream os(prefix);
                    os << d.file << ":" << d.line << ":" << d.col << ": ";
                }

                // d.level is already Error/Fatal (you said you filtered)
                unsigned id = DE_.getCustomDiagID(d.level, "%0%1");
                (DE_.Report(id) << prefix << d.msg);

                if (d.level >= clang::DiagnosticsEngine::Error)
                {
                    ++emittedErrors;
                }
            }

            // Safety tripwire (optional): ensure a nonzero exit even if frozen
            // was empty.
            if (frozen.empty() || emittedErrors == 0)
            {
                unsigned id = DE_.getCustomDiagID(
                    clang::DiagnosticsEngine::Error,
                    "errors occurred (deferred)");
                DE_.Report(id);
            }

            Replaying_ = false;
        }

        Downstream_->EndSourceFile();
    }


    void
    finish() override
    {
        if (Downstream_)
        {
            Downstream_->finish();
        }
    }
};

} // namespace mrdocs
} // namespace clang

#endif
