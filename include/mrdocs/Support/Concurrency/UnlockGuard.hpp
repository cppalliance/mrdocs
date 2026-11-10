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

#ifndef MRDOCS_API_SUPPORT_CONCURRENCY_UNLOCKGUARD_HPP
#define MRDOCS_API_SUPPORT_CONCURRENCY_UNLOCKGUARD_HPP

#include <mrdocs/Platform.hpp>
#include <mutex>


namespace mrdocs {

/** A scoped guard which unlocks a mutex.
*/
class MRDOCS_VISIBLE UnlockGuard
{
    std::mutex& m_;

public:
    /** Destructor.
    */
    ~UnlockGuard()
    {
        m_.lock();
    }

    /** Constructor.
    */
    explicit UnlockGuard(std::mutex& m)
        : m_(m)
    {
        m_.unlock();
    }
};

} // mrdocs


#endif // MRDOCS_API_SUPPORT_CONCURRENCY_UNLOCKGUARD_HPP
