//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_RAWOSTREAM_HPP
#define MRDOX_SUPPORT_RAWOSTREAM_HPP

#include <llvm/Support/raw_ostream.h>
#include <ostream>

namespace clang {
namespace mrdox {

class RawOstream : public llvm::raw_ostream
{
    std::ostream& os_;

public:
    explicit
    RawOstream(
        std::ostream& os) noexcept
        : os_(os)
    {
    }

    ~RawOstream()
    {
        flush();
    }

private:
    void
    write_impl(
        const char *Ptr, size_t Size) override
    {
        os_.write(Ptr, Size);
    }

    virtual
    uint64_t
    current_pos() const override
    {
        return 0;
    }
};

} // mrdox
} // clang

#endif
