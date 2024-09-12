//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_TAGFILEWRITER_HPP
#define MRDOCS_LIB_TAGFILEWRITER_HPP

#include "../Gen/xml/XMLTags.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <string>

namespace clang {
namespace mrdocs {

struct SimpleWriterTag {}; //Tag dispatch for simple writers

class jit_indenter;

/** A writer which outputs Tagfiles.
*/
class TagfileWriter
{
protected:
    xml::XMLTags tags_;
    llvm::raw_ostream& os_;
    Corpus const& corpus_;

public:
    TagfileWriter(
        llvm::raw_ostream& os,
        Corpus const& corpus) noexcept;

    void initialize();
    void finalize();

    void writeIndex();

    template<class T>
    void operator()(T const&, std::string_view, std::size_t);

    template<class T>
    void operator()(T const&, std::string_view, std::size_t, SimpleWriterTag);

    bool containsOnlyNamespaces(NamespaceInfo const& I) const;    

#define INFO_PASCAL(Type) void write##Type(Type##Info const&, std::string_view, std::size_t);
#include <mrdocs/Metadata/InfoNodes.inc>

#define INFO_PASCAL(Type) void write##Type(Type##Info const&, std::string_view, std::size_t, SimpleWriterTag);
#include <mrdocs/Metadata/InfoNodes.inc>

    template<class T>
    void writeClassLike(T const& I, std::string_view, std::size_t);
    template<class T>
    void writeClassLike(T const& I, std::string_view, std::size_t, SimpleWriterTag);

    void openTemplate(std::unique_ptr<TemplateInfo> const& I);
    void closeTemplate(std::unique_ptr<TemplateInfo> const& I);
};

} // mrdocs
} // clang

#endif
