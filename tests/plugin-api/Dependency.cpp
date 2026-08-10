//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The shared library WithDependency.cpp links: an ordinary library that
// knows nothing of MrDocs, standing for the ones a real plugin brings
// along, such as SQLite.

int
pluginDependencyAnswer()
{
    return 42;
}
