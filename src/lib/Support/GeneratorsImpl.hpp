//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_GENERATORSIMPL_HPP
#define MRDOCS_LIB_SUPPORT_GENERATORSIMPL_HPP

#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Generators.hpp>
#include <llvm/ADT/SmallVector.h>
#include <memory>
#include <vector>

namespace clang {
namespace mrdocs {

/** Implementaiton of Generators.
*/
class MRDOCS_VISIBLE
    GeneratorsImpl : public Generators
{
    llvm::SmallVector<Generator const*, 3> plist_;
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
    Error
    insert(std::unique_ptr<Generator> G);
};

/** Return a reference to the global GeneratorsImpl instance.
*/
GeneratorsImpl&
getGeneratorsImpl() noexcept;

} // mrdocs
} // clang

#endif
