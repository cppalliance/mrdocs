//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#ifndef MRDOCS_TEST_SUPPORT_TESTLAYOUT_HPP
#define MRDOCS_TEST_SUPPORT_TESTLAYOUT_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <llvm/ADT/StringRef.h>
#include <string>
#include <memory>

namespace mrdocs {

class ScopedTempDirectory;
enum Action : int;

enum class OutputMode
{
    SinglePage,
    Multipage
};

/** Expected output layout for a single test (single-page or multipage). */
struct TestLayout
{
    OutputMode mode = OutputMode::SinglePage;
    std::string expectedSinglePath;
    std::string multipageRoot;
    std::string multipageFormatRoot;
    std::string generatedOutputRoot;
    std::unique_ptr<ScopedTempDirectory> tempDir;
    bool hasFileConfig = false;
};

struct ResolvedLayout
{
    Config::Settings settings;
    TestLayout layout;
};

/** Resolve per-test settings + layout, enforcing single vs multipage rules. */
Expected<ResolvedLayout>
resolveTestLayout(
    llvm::StringRef filePath,
    Config::Settings const& dirSettings,
    llvm::StringRef generatorExtension,
    ReferenceDirectories const& dirs,
    Action action);

} // namespace mrdocs

#endif
