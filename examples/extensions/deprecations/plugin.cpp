//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A MrDocs plugin that reports what the code marks as deprecated. It
// installs a generator, which lists every symbol that carries
// `[[deprecated]]` with its message and its location, and a transform, which
// uses the message as the brief of a deprecated symbol that has none.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Transform.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

// tag::attribute[]
// The message of the `[[deprecated]]` attribute of a symbol: nullopt for a
// symbol that is not deprecated, and an empty string for one that is
// deprecated without giving a reason.
std::optional<std::string>
deprecationMessage(mrdocs::Symbol const& symbol)
{
    for (auto const& attribute : symbol.Attributes)
    {
        if (attribute->isDeprecated())
        {
            return attribute->asDeprecated().Message;
        }
    }
    return std::nullopt;
}
// end::attribute[]

// Whether the documentation shows the symbol: a symbol extracted as `regular`
// or `see-below` (which has a page that shows its type as "see below"). That
// leaves out two kinds of symbol. One is extracted only because a documented
// symbol uses it (a `dependency`, such as a base class from another library).
// The other is a symbol the configuration hides as `implementation-defined`
// (such as one in a `detail` namespace). The copy of a base class member that
// a derived class lists is shown, and so it counts. tag::shown[]
bool
isShown(mrdocs::Symbol const& symbol)
{
    return symbol.Extraction == mrdocs::ExtractionMode::Regular ||
        symbol.Extraction == mrdocs::ExtractionMode::SeeBelow;
}
// end::shown[]

// Whether the report lists the symbol: a symbol the documentation shows, other
// than a copy of a base class member made for a derived class. The copy would
// point at the line of the derived class, and the member it was made from is
// reported where it is declared.
// tag::shown[]
bool
isReported(mrdocs::Symbol const& symbol)
{
    return isShown(symbol) && !symbol.InheritedFrom;
}
// end::shown[]

// The template information of a symbol that can be a specialization.
mrdocs::TemplateInfo const*
templateInfo(mrdocs::Symbol const& symbol)
{
    if (auto const* record = symbol.asRecordPtr())
    {
        return record->Template ? &*record->Template : nullptr;
    }
    if (auto const* function = symbol.asFunctionPtr())
    {
        return function->Template ? &*function->Template : nullptr;
    }
    if (auto const* variable = symbol.asVariablePtr())
    {
        return variable->Template ? &*variable->Template : nullptr;
    }
    return nullptr;
}

// The name of a symbol, followed by its template arguments when it is an
// explicit or partial specialization: `Box<int>` and not `Box`, which is the
// name of the primary template. A symbol with no name is "(anonymous)".
std::string
nameWithArguments(mrdocs::Symbol const& symbol)
{
    std::string result = symbol.Name.empty() ? "(anonymous)" : symbol.Name;
    mrdocs::TemplateInfo const* info = templateInfo(symbol);
    if (info && !info->Args.empty())
    {
        result += '<';
        bool first = true;
        for (auto const& arg : info->Args)
        {
            if (!first)
            {
                result += ", ";
            }
            first = false;
            result += mrdocs::toString(*arg);
        }
        result += '>';
    }
    return result;
}

// The name of a symbol with the names of the scopes around it, as written in
// code, with the template arguments of a specialization. The walk ends at the
// global namespace, which is not part of the name. MrDocs gives a member of an
// anonymous namespace the enclosing namespace as its parent, so the walk never
// meets one.
// tag::qualified-name[]
std::string
qualifiedName(mrdocs::Corpus const& corpus, mrdocs::Symbol const& symbol)
{
    std::string result = nameWithArguments(symbol);
    mrdocs::Symbol const* current = &symbol;
    while (true)
    {
        mrdocs::Symbol const* scope = corpus.find(current->Parent);
        if (!scope || scope->Parent == mrdocs::SymbolID::invalid)
        {
            break;
        }
        result = nameWithArguments(*scope) + "::" + result;
        current = scope;
    }
    return result;
}
// end::qualified-name[]

// One deprecated symbol of the report.
struct Entry
{
    std::string name;
    std::string kind;
    std::string path;
    unsigned line = 0;
    std::string message;
};

// A generator that lists the deprecated symbols, one entry each, ordered by
// name.
class DeprecationsGenerator final
    : public mrdocs::Generator
{
public:
    std::string_view
    id() const noexcept override
    {
        return "deprecations";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "Deprecation report";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "txt";
    }

    mrdocs::Expected<void>
    build(
        mrdocs::Corpus const& corpus,
        mrdocs::Config const& config,
        mrdocs::report::Reporter& reporter) const override;
};

mrdocs::Expected<void>
DeprecationsGenerator::
build(
    mrdocs::Corpus const& corpus,
    mrdocs::Config const& config,
    mrdocs::report::Reporter&) const
{
// tag::collect[]
    std::vector<Entry> entries;
    for (mrdocs::Symbol const& symbol : corpus)
    {
// end::collect[]
        // An overload set is deprecated when all its members are, and the
        // members are reported on their own.
// tag::collect[]
        if (symbol.isOverloads() || !isReported(symbol))
        {
            continue;
        }
        std::optional<std::string> message = deprecationMessage(symbol);
        if (!message)
        {
            continue;
        }
// end::collect[]
        // The location MrDocs shows on the page of the symbol: a declaration,
        // or the definition for a class or enumeration and for a symbol with
        // no declaration. A symbol with no location, or one in a file that is
        // not under `source-root`, has no path.
// tag::collect[]
        auto const where = mrdocs::getPrimaryLocation(symbol);
        entries.push_back({
            qualifiedName(corpus, symbol),
            std::string(mrdocs::toString(symbol.Kind)),
            where ? where->SourcePath : std::string(),
            where ? where->LineNumber : 0u,
            *message});
    }
// end::collect[]
    // The line is a number, so that line 9 comes before line 12.
    std::sort(entries.begin(), entries.end(),
        [](Entry const& a, Entry const& b)
        {
            return std::tie(a.name, a.path, a.line) <
                std::tie(b.name, b.path, b.line);
        });

    std::filesystem::path dir(config.configDir());
    dir /= config.output;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::filesystem::path const file = dir / "deprecations.txt";

    // Binary mode keeps the line endings the same on every platform.
    std::ofstream os(file, std::ios::binary);
    if (!os)
    {
        return mrdocs::Unexpected(mrdocs::formatError(
            "could not open \"{}\" for writing", file.string()));
    }
// tag::write[]
    for (Entry const& entry : entries)
    {
        // A symbol with no known location has no " at " part.
        os << entry.name << " (" << entry.kind << ')';
        if (!entry.path.empty())
        {
            os << " at " << entry.path << ':' << entry.line;
        }
        os << '\n';
        if (entry.message.empty())
        {
            os << "    no message\n";
        }
        else
        {
            os << "    " << entry.message << '\n';
        }
    }
// end::write[]
    // A failed write (a full disk, say) shows in the stream state only.
    os.flush();
    if (!os)
    {
        return mrdocs::Unexpected(mrdocs::formatError(
            "could not write \"{}\"", file.string()));
    }
    return {};
}

// A transform that uses the message of its attribute as the brief of a
// deprecated symbol that has none. The output then says why the symbol is
// deprecated even where the author did not write it down.
class DeprecationBrief final
    : public mrdocs::Transform
{
public:
    std::string_view
    id() const noexcept override
    {
        return "deprecation-brief";
    }

    mrdocs::Expected<void>
    apply(
        mrdocs::Corpus& corpus,
        mrdocs::Config const& config,
        mrdocs::report::Reporter& reporter) const override;
};

mrdocs::Expected<void>
DeprecationBrief::
apply(
    mrdocs::Corpus& corpus,
    mrdocs::Config const&,
    mrdocs::report::Reporter&) const
{
// tag::brief-select[]
    for (mrdocs::Symbol const& symbol : corpus)
    {
// end::brief-select[]
        // The copy of a base class member that a derived class lists gets a
        // brief like any other member, but a symbol the documentation does
        // not show does not.
// tag::brief-select[]
        if (!isShown(symbol))
        {
            continue;
        }
// end::brief-select[]

        // An overload set has the attributes its members share. MrDocs made
        // its brief from the members before the transforms ran, so the set
        // needs a brief of its own, because the namespace page lists it in a
        // row of its own.
// tag::brief-select[]
        std::optional<std::string> message = deprecationMessage(symbol);
        if (!message)
        {
            continue;
        }
// end::brief-select[]

        // A corpus is walked as const. Changing a symbol means asking for it
        // by id, which is where the corpus hands out a mutable reference.
// tag::brief-write[]
        mrdocs::Symbol* const target = corpus.find(symbol.id);
        if (!target)
        {
            continue;
        }
        if (!target->doc)
        {
            target->doc.emplace();
        }
        if (!target->doc->brief)
        {
            target->doc->brief = mrdocs::doc::BriefBlock(
                message->empty() ? "Deprecated." : "Deprecated: " + *message);
        }
    }
// end::brief-write[]
    return {};
}

} // (anon)

// tag::main[]
MRDOCS_PLUGIN_MAIN(plugin)
{
    mrdocs::Expected<void> added = plugin.registerGenerator(
        std::make_unique<DeprecationsGenerator>());
    if (!added)
    {
        return added;
    }
    return plugin.registerTransform(std::make_unique<DeprecationBrief>());
}
// end::main[]
