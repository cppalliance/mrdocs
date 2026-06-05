//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//
//
// The previous content here exercised the now-removed `setMemberImpl`
// + `ExtensionState::byId` machinery, which has been replaced by the
// templated `DescribedObjectProxy` (writes go through reflection on
// the live C++ Symbol). The behavioral coverage moved into the
// golden tests under `test-files/golden-tests/extensions/`, which
// exercise direct assignment from both JS and Lua against a real
// `CorpusImpl`. Leaving an empty TU here keeps the rest of the test
// binary building without a separate CMake list change.

#include <test_suite/test_suite.hpp>

namespace mrdocs {
namespace {

struct SetMemberTest
{
    void
    run()
    {
        // Behavior exercised by golden tests; nothing to do here.
    }
};

TEST_SUITE(SetMemberTest, "mrdocs.Extensions.SetMember");

} // (anon)
} // mrdocs
