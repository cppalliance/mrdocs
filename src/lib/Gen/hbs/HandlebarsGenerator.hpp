//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HBS_HANDLEBARSGENERATOR_HPP
#define MRDOCS_LIB_GEN_HBS_HANDLEBARSGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <lib/Gen/hbs/HandlebarsCorpus.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

class OutputRef;

namespace hbs {

/** Pattern-replacement table used to escape rendered output values.

    A single-byte source is stored in a 256-entry array indexed by the
    `unsigned char` value of the source. A multi-byte source goes into a
    bucket keyed by its first byte; each bucket is empty for bytes that
    have no multi-byte rule, so the walk pays nothing for the multi-byte
    machinery in the common case. When the bucket is non-empty, the
    longest matching pattern wins; if no multi-byte pattern matches at
    the current position, the single-byte rule (if any) applies. A byte
    with no rule at all passes through unchanged.

    Multi-byte support exists so that a format can distinguish, e.g.,
    Markdown's `**bold**` from a literal `*`, or RST's ``` ``literal`` ```
    from `*emphasis*`. It also accommodates UTF-8 codepoints past ASCII,
    which are indexed by byte everywhere else but want to be replaced as
    a whole.
*/
class EscapeMap
{
    // Single-byte rules. Index by `unsigned char`; empty string means
    // "no rule for this byte" (pass through).
    std::array<std::string, 256> singleByte_;

    // Multi-byte rules bucketed by first byte. Each bucket holds
    // (pattern, replacement) pairs where `pattern.size() >= 2` and
    // `pattern[0]` equals the bucket index. Buckets are typically
    // empty, so the walk's "is there anything to check here?" test is
    // a single null check per input byte.
    std::array<std::vector<std::pair<std::string, std::string>>, 256>
        multiByte_;

public:
    /** Replace a single byte with `replacement` whenever it appears
        in escaped text.

        @param c The byte to replace.
        @param replacement The string to emit in its place.
    */
    void
    set(char c, std::string_view replacement)
    {
        singleByte_[static_cast<unsigned char>(c)] = replacement;
    }

    /** Replace a pattern of one or more bytes with `replacement`
        whenever it appears in escaped text.

        Single-byte patterns are stored on the fast single-byte path
        and behave identically to `set(char, string_view)`. Multi-byte
        patterns are stored in a bucket keyed by their first byte;
        when an existing pattern with the same source is set again,
        the replacement is updated in place.

        Behavior is undefined when `source` is empty.

        @param source The byte sequence to replace.
        @param replacement The string to emit in its place.
    */
    void
    set(std::string_view source, std::string_view replacement);

    /** Append the escaped form of `str` to `out`.
    */
    void
    apply(OutputRef& out, std::string_view str) const;
};

class HandlebarsGenerator
    : public Generator
{
    std::string id_;
    std::string fileExtension_;
    std::string displayName_;
    std::string extends_;

public:
    struct StylesheetRef
    {
        /** Absolute path to the source stylesheet on disk (empty for remote). */
        std::string sourcePath;
        /** Href or output-relative path written to the HTML. */
        std::string outputRelative;
        /** True when the stylesheet is remote and should not be copied. */
        bool external = false;
    };

    struct StylesData
    {
        std::vector<StylesheetRef> stylesheets;
        std::vector<std::string> inlineStyles;
        std::vector<std::string> inlineScripts;
        bool hasDefaultStyles = false;
    };

protected:
    /** Escape table for rendered output. Subclasses populate it in
        their constructor; the base class drives `escape()` from it.
    */
    EscapeMap escapeMap_;

public:
    /** Construct a Handlebars-based generator from data.

        Used both by the built-in subclasses (which pass their fixed
        identity strings and populate `escapeMap_` in the body) and by
        the addon-discovery path, which can build a generator entirely
        from a template directory without writing a new C++ subclass.

        @param id Stable identifier (matches `mrdocs.yml`'s `generator:`).
        @param fileExtension Output file extension (e.g. "html", "adoc").
        @param displayName Human-readable name shown in messages.
        @param escapeMap Character-replacement table; empty means
                        rendered output passes through unchanged.
        @param extends Id of the generator this one inherits partials
                       and helpers from. Empty means no inheritance;
                       only `common/` is consulted as a fallback.
    */
    HandlebarsGenerator(
        std::string const& id,
        std::string const& fileExtension,
        std::string const& displayName,
        EscapeMap escapeMap = {},
        std::string extends = {});

    /** Id of the generator this one inherits partials and helpers from.

        Empty when the generator stands alone (the default, used by
        the built-in `adoc` and `html` generators). When non-empty,
        the Builder walks `<root>/generator/<extends>/{partials,helpers}/`
        in addition to `common/` and the generator's own directory,
        so an addon can ship only the partials it overrides and inherit
        the rest from a parent format. Layouts do not inherit; each
        format owns its `index.<id>.hbs` and `wrapper.<id>.hbs`.
    */
    std::string_view
    extends() const noexcept
    {
        return extends_;
    }

    std::string_view
    id() const noexcept override
    {
        return id_;
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return fileExtension_;
    }

    std::string_view
    displayName() const noexcept override
    {
        return displayName_;
    }

    Expected<void>
    build(Corpus const& corpus) const override;

    /** Build a tagfile for the corpus.
    */
    Expected<void>
    buildTagfile(
        std::ostream& os,
        Corpus const& corpus) const;

    /** Build a tagfile for the corpus and store the result in a file.
    */
    Expected<void>
    buildTagfile(
        std::string_view fileName,
        Corpus const& corpus) const;

    /** Append the escaped form of `str` to `os`.

        The default implementation drives the result from the
        generator's `escapeMap_`, which is the path used by
        data-driven generators (their map comes from
        `mrdocs-generator.yml`). The built-in `adoc` and `html`
        generators override this with their own hand-written
        switches; the array lookup the default uses is slightly
        slower than a compiled switch, and those generators are
        on the hot path.
    */
    virtual
    void
    escape(OutputRef& os, std::string_view str) const;

protected:
    /** Customize the Handlebars corpus before rendering.

        Subclasses can override this to inject additional data
        into the corpus (e.g. precomputed template context).
        The default implementation does nothing.
    */
    virtual
    void
    prepareCorpus(HandlebarsCorpus&) const;

protected:
    /** Default stylesheet path on disk; empty means no default. */
    virtual std::string defaultStylesheetSource(Config const& config) const;

    /** Default stylesheet output relative path; empty means no default. */
    virtual std::string defaultStylesheetOutput(Config const& config) const;

    /** Default highlight stylesheet path on disk; empty means no default. */
    virtual std::string defaultHighlightStylesheetSource(Config const& config) const;

    /** Default highlight stylesheet output relative path; empty means no default. */
    virtual std::string defaultHighlightStylesheetOutput(Config const& config) const;

    /** Inline script used to load and run highlight.js from a CDN. */
    virtual std::string defaultHighlightScript() const;

private:
    // Collect the stylesheets and scripts a page references, as data only
    // (no files are written). See the .cpp for details.
    Expected<StylesData>
    prepareStylesheets(Config const& config) const;

    // Copy the non-external stylesheet files into `outputDir`, when the
    // configuration asks for linked-and-copied CSS. Called by build() once
    // the generator's output directory is known.
    Expected<void>
    copyStylesheets(Config const& config, std::string_view outputDir) const;

    // Render the single-page form of the documentation to a stream.
    // build() drives this, wrapping it with the file it opens.
    Expected<void>
    renderSinglePage(std::ostream& os, Corpus const& corpus) const;
};

} // hbs

} // mrdocs

#endif
