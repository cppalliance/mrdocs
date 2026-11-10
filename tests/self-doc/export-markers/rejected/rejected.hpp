//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// MRDOCS_VISIBLE on a member and on a function. `type_visibility` applies to
// types only, so the parse fails and the `mrdocs-self-doc-export-markers`
// test expects that failure, whichever marker the declaration needs.

#include <mrdocs/Support/Export.hpp>

namespace mrdocs {

class MRDOCS_VISIBLE VisibleMember
{
public:
    MRDOCS_VISIBLE void run();
    MRDOCS_VISIBLE int count() const { return 0; }
};

MRDOCS_VISIBLE void freeVisible();

} // mrdocs
