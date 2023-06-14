//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_ADOC_ADOCSINGLEPAGEWRITER_HPP
#define MRDOX_TOOL_ADOC_ADOCSINGLEPAGEWRITER_HPP

#include "AdocWriter.hpp"
#include "Support/SafeNames.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Platform.hpp>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocSinglePageWriter
    : public AdocWriter
    , public Corpus::Visitor
{
    SafeNames names_;

public:
    AdocSinglePageWriter(
        llvm::raw_ostream& os,
        Corpus const& corpus) noexcept;

    Error build();

private:
    /** Return an array of info pointers display-sorted by symbol.
    */
    template<class Type>
    std::vector<Type const*>
    buildSortedList(
        std::vector<SymbolID> const& from) const;

    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visitOverloads(OverloadInfo const&);
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;
    bool visit(VariableInfo const&) override;

    //bool visitOverloads(Info const& P, OverloadInfo const&);
};

} // adoc
} // mrdox
} // clang

#endif
