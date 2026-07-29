//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TEST_GOLDEN_ACTION_HPP
#define MRDOCS_TEST_GOLDEN_ACTION_HPP

namespace mrdocs {

/** What a golden-test run does with each fixture. */
enum Action : int
{
    /// Compare generated output against the fixture; a mismatch or a missing
    /// fixture fails the run.
    test,
    /// Write the fixture when it is missing; otherwise behave like `test`.
    create,
    /// Rewrite the fixture from the generated output.
    update,
};

} // mrdocs

#endif // MRDOCS_TEST_GOLDEN_ACTION_HPP
