//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_ADOC_DOCVISITOR_HPP
#define MRDOCS_LIB_GEN_ADOC_DOCVISITOR_HPP

#include <mrdocs/Platform.hpp>
#include "lib/Gen/hbs/HandlebarsCorpus.hpp"
#include <optional>

namespace clang {
namespace mrdocs {
namespace adoc {

class DocVisitor
{
    hbs::HandlebarsCorpus const& corpus_;
    std::string& dest_;
    std::back_insert_iterator<std::string> ins_;

    template<typename Fn>
    bool
    write(
        const doc::Node& node,
        Fn&& fn)
    {
        const auto n_before = dest_.size();
        doc::visit(node, std::forward<Fn>(fn));
        return dest_.size() != n_before;
    }

public:
    DocVisitor(
        hbs::HandlebarsCorpus const& corpus,
        std::string& dest) noexcept
        : corpus_(corpus)
        , dest_(dest)
        , ins_(std::back_inserter(dest_))
    {
    }

    void operator()(doc::Admonition const& I);
    void operator()(doc::Code const& I);
    void operator()(doc::Heading const& I);
    void operator()(doc::Paragraph const& I);
    void operator()(doc::Link const& I);
    void operator()(doc::ListItem const& I);
    void operator()(doc::Param const& I);
    void operator()(doc::Returns const& I);
    void operator()(doc::Text const& I);
    void operator()(doc::Styled const& I);
    void operator()(doc::TParam const& I);
    void operator()(doc::Reference const& I);
    void operator()(doc::Throws const& I);

    std::size_t measureLeftMargin(
        doc::List<doc::Text> const& list);
};

} // hbs
} // mrdocs
} // clang

#endif
