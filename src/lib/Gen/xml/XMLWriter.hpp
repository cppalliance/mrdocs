//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_XML_XMLWRITER_HPP
#define MRDOCS_LIB_GEN_XML_XMLWRITER_HPP

#include <lib/Gen/xml/XMLTags.hpp>
#include <mrdocs/Corpus.hpp>
#include <llvm/Support/raw_ostream.h>
#include <concepts>
#include <string_view>

namespace mrdocs::xml {

class jit_indenter;

/** A writer that outputs XML.
*/
class XMLWriter
{
    XMLTags tags_;
    llvm::raw_ostream& os_;
    Corpus const& corpus_;

public:
    XMLWriter(
        llvm::raw_ostream& os,
        Corpus const& corpus) noexcept;

    Expected<void>
    build();

    template <std::derived_from<Symbol> SymbolTy>
    void
    operator()(SymbolTy const& I);

private:
    template <typename T> void write(T const& value);
    template <typename T> void writeElement(std::string_view tag, T const& value);
    template <typename T> void writeMembers(T const& obj);
    template <typename T> void writePolymorphic(T const& value);
};

} // mrdocs::xml

#endif // MRDOCS_LIB_GEN_XML_XMLWRITER_HPP
