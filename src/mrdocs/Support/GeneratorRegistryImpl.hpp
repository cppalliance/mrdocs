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
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <llvm/ADT/SmallVector.h>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>


namespace mrdocs {

/** The global, concrete registry of @ref Generator elements.
*/
class MRDOCS_VISIBLE
    GeneratorRegistryImpl
{
    mutable std::mutex mutex_;
    llvm::SmallVector<Generator const*, 3> plist_;
    llvm::SmallVector<
        std::unique_ptr<Generator>> list_;

    void refresh_plist();

public:
    /// Iterator over the registered generators.
    using iterator = Generator const* const*;

    GeneratorRegistryImpl();

    iterator
    begin() const noexcept
    {
        return plist_.begin();
    }

    iterator
    end() const noexcept
    {
        return plist_.end();
    }

    Generator const*
    find(
        std::string_view name) const noexcept;

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
