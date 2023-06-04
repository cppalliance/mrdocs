//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_GENERATORSIMPL_HPP
#define MRDOX_SUPPORT_GENERATORSIMPL_HPP

#include <mrdox/Error.hpp>
#include <mrdox/Platform.hpp>
#include <mrdox/Generators.hpp>
#include <llvm/ADT/SmallVector.h>
#include <memory>
#include <vector>

namespace clang {
namespace mrdox {

/** Implementaiton of Generators.
*/
class MRDOX_VISIBLE
    GeneratorsImpl : public Generators
{
    llvm::SmallVector<Generator const*, 8> plist_;
    llvm::SmallVector<
        std::unique_ptr<Generator>> list_;

    void refresh_plist();

public:
    GeneratorsImpl();

    iterator
    begin() const noexcept override
    {
        return plist_.begin();
    }

    iterator
    end() const noexcept override
    {
        return plist_.end();
    }

    Generator const*
    find(
        std::string_view name) const noexcept override;

    /** Insert a generator
    */
    llvm::Error
    insert(std::unique_ptr<Generator> G);
};

/** Return a reference to the global GeneratorsImpl instance.
*/
GeneratorsImpl&
getGeneratorsImpl() noexcept;

} // mrdox
} // clang

#endif
