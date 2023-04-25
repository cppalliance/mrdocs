//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_ADOC_ADOCSINGLEPAGEWRITER_HPP
#define MRDOX_API_ADOC_ADOCSINGLEPAGEWRITER_HPP

#include "AdocWriter.hpp"
#include "SafeNames.hpp"
#include <mrdox/Corpus.hpp>

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
        llvm::raw_fd_ostream* fd_os,
        Corpus const& corpus,
        Reporter& R) noexcept;

    llvm::Error build();

private:
    /** Return an array of info pointers display-sorted by symbol.
    */
    template<class Type>
    std::vector<Type const*>
    buildSortedList(
        std::vector<Reference> const& from) const;

    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;
};

} // adoc
} // mrdox
} // clang

#endif
