//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#ifndef MRDOCS_TEST_SUPPORT_COMPARISON_HPP
#define MRDOCS_TEST_SUPPORT_COMPARISON_HPP

#include "TestLayout.hpp"
#include <mrdocs/Support/Error.hpp>
#include <string>
#include <llvm/Support/ErrorOr.h>

namespace mrdocs {

enum Action : int;
struct TestResults;
class Generator;
class Corpus;
class ConfigImpl;

namespace test_support {

/** Arguments needed for single-page comparison flow. */
struct SinglePageArgs
{
    TestLayout const& layout;
    Generator const& gen;
    Corpus const& corpus;
    std::string_view filePath;
    Action action;
    bool writeBad;
    bool forceUpdate;
    ReferenceDirectories const& dirs;
    TestResults& results;
};

/** Arguments needed for multipage comparison flow. */
struct MultipageArgs
{
    TestLayout const& layout;
    Generator const& gen;
    Corpus const& corpus;
    Action action;
    bool forceUpdate;
    TestResults& results;
};

/** Compare generated single-page output against expected file. */
Expected<void>
compareSinglePage(SinglePageArgs const& args);

/** Compare generated multipage output directory against snapshot tree. */
Expected<void>
compareMultipage(MultipageArgs const& args);

} // namespace test_support
} // namespace mrdocs

#endif
