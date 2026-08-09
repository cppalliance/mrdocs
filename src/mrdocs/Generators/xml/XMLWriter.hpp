//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GENERATORS_XML_XMLWRITER_HPP
#define MRDOCS_LIB_GENERATORS_XML_XMLWRITER_HPP

#include "XMLTags.hpp"
#include <mrdocs/Corpus.hpp>
#include <llvm/Support/raw_ostream.h>
#include <concepts>
#include <string_view>

namespace mrdocs::xml {

class jit_indenter;

/** A writer that outputs XML.

    The output is driven entirely by the metadata's compile-time reflection
    (Boost.Describe); the writer walks each value with `if constexpr` and emits
    it by these rules:

    @li Field names use camelCase (the member name with its first letter
        lowered).
    @li Leaves use the built-in XSD datatypes (xsd:integer, xsd:string), as
        declared in `schemas/generators/mrdocs.rng`.
    @li A boolean is written as presence: `true` is an empty element
        (`<isVariadic/>`) and `false` is omitted entirely, since an absent
        element reads as false.
    @li A field with no value is omitted: an empty Optional, an empty string or
        list, an invalid SymbolID, a `false` boolean, or a scalar whose rendered
        text is empty (so `0` stays, but a valueless enum such as access "none"
        drops).
    @li An object emits each field recursively as a child element whose tag is
        the field name.
    @li An object all of whose fields are omitted is itself omitted, rather than
        written as an empty element: a value counts as omitted when the rules
        above drop it (a `false` boolean, an empty string or list, an invalid
        SymbolID, ...), applied recursively, so an object with no children never
        appears.
    @li A scalar emits its value as the element's text.
    @li A list of token scalars (no whitespace: ids, integers, enums) is one
        element with the values space-separated; a list of whitespace-capable
        scalars follows the non-scalar list rule with `<string>` entries.
    @li A non-scalar list is a wrapper named for the field, with one child per
        entry tagged by the entry's concrete type.
    @li A variant is resolved to its concrete type: as a list entry it is
        emitted under that type's tag; as a field it is wrapped twice (outer
        tag the field name, inner tag the concrete type).
    @li An object whose whole content is a single string member collapses to
        that string (e.g. `ExprInfo`, whose only field is its written source,
        is emitted as text). An object with more than one member keeps the
        normal form.
*/
class XMLWriter
{
    XMLTags tags_;
    llvm::raw_ostream& os_;
    Corpus const& corpus_;

public:
    /** Construct a writer that emits the XML for a corpus.

        @param os The stream that receives the XML output.
        @param corpus The corpus whose symbols are written.
    */
    XMLWriter(
        llvm::raw_ostream& os,
        Corpus const& corpus) noexcept;

    /** Write the whole corpus as an XML document.

        Emits the prolog and the root `<mrdocs>` element, then the global
        namespace and, transitively, every symbol it contains.

        @return An error if the document could not be written.
    */
    Expected<void>
    build();

    /** Write one symbol, then recurse into the symbols it contains.

        Invoked for each symbol by the corpus traversal. The symbol is emitted
        as a flat top-level element tagged by its type; the tree structure is
        recovered from the SymbolID references in its fields.

        @param I The symbol to write.
    */
    template <std::derived_from<Symbol> SymbolTy>
    void
    operator()(SymbolTy const& I);

private:
    // Emit `value` as a `<tag>` element, with its reflected fields as children.
    template <typename T>
    void
    writeObject(const std::string& tag, T const& value);

    // Emit each reflected member of `obj` (inherited members included) as a field.
    template <typename T>
    void
    writeObjectBody(T const& obj);

    // Emit member `value` as a `<tag>` field, its form chosen from the type, or
    // omit the field entirely when it has no value.
    template <typename T>
    void
    writeObjectField(std::string_view tag, T const& value);

    // Emit the text content of a leaf `value` to `os`, with no surrounding tags.
    template <typename T>
    void
    writeScalar(llvm::raw_ostream& os, T const& value);

    // Emit a `value` that could be another object, an array, or a leaf
    template <typename T>
    void
    writeValue(T const& value);
};

} // mrdocs::xml

#endif // MRDOCS_LIB_GENERATORS_XML_XMLWRITER_HPP
