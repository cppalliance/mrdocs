//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_ADOC_ADOCMULTIPAGEWRITER_HPP
#define MRDOX_API_ADOC_ADOCMULTIPAGEWRITER_HPP

#include "AdocWriter.hpp"
#include "SafeNames.hpp"
#include <mrdox/Corpus.hpp>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocMultiPageWriter
    : public AdocWriter
{
    SafeNames const& names_;

public:
    AdocMultiPageWriter(
        llvm::raw_ostream& os,
        llvm::raw_fd_ostream* fd_os,
        Corpus const& corpus,
        SafeNames const& names,
        Reporter& R) noexcept;

    void build(NamespaceInfo const&);
    void build(RecordInfo const&);
    void build(FunctionInfo const&);
    void build(TypedefInfo const&);
    void build(EnumInfo const&);

    void build(OverloadInfo const&);

private:
    void writeTitle(Info const& I);

    llvm::StringRef linkFor(Info const& I) override;
};

} // adoc
} // mrdox
} // clang

#endif
