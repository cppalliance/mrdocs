//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_GENERATORREGISTRYIMPL_HPP
#define MRDOCS_LIB_SUPPORT_GENERATORREGISTRYIMPL_HPP

#include <mrdocs/Platform.hpp>
#include <lib/Support/GeneratorRegistry.hpp>
#include <mrdocs/Support/Error.hpp>
#include <llvm/ADT/SmallVector.h>
#include <memory>
#include <mutex>
#include <vector>


namespace mrdocs {

/** Implementation of GeneratorRegistry.
*/
class MRDOCS_VISIBLE
    GeneratorRegistryImpl : public GeneratorRegistry
{
    mutable std::mutex mutex_;
    llvm::SmallVector<Generator const*, 3> plist_;
    llvm::SmallVector<
        std::unique_ptr<Generator>> list_;

    void refresh_plist();

public:
    GeneratorRegistryImpl();

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
    Expected<void>
    insert(std::unique_ptr<Generator> G);
};

/** Return a reference to the global GeneratorRegistryImpl instance.
*/
GeneratorRegistryImpl&
getGeneratorRegistryImpl() noexcept;

} // mrdocs


#endif
