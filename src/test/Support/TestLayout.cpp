//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#include "TestLayout.hpp"
#include <lib/Support/Path.hpp>
#include <lib/Support/Report.hpp>
#include <mrdocs/Support/Error.hpp>
#include "../TestArgs.hpp"
#include <llvm/Support/Path.h>

namespace mrdocs {

namespace {

/** Replace the extension on a path and return the new small string. */
SmallPathString
pathWithExtension(
    llvm::StringRef path,
    llvm::StringRef ext)
{
    SmallPathString result(path);
    llvm::sys::path::replace_extension(result, ext);
    return result;
}

} // (anon)

/** Read any per-file mrdocs.yml on top of the directory-level settings. */
Expected<LoadedTestSettings>
loadTestSettings(
    llvm::StringRef filePath,
    Config::Settings const& dirSettings,
    ReferenceDirectories const& dirs)
{
    LoadedTestSettings result;
    result.settings = dirSettings;
    result.dirMultipage = dirSettings.multipage;
    std::string const configPath = files::withExtension(filePath, "yml");
    result.hasFileConfig = files::exists(configPath);
    if (result.hasFileConfig)
    {
        Expected<void> const exp = Config::Settings::load_file(
            result.settings, configPath, dirs);
        if (!exp)
        {
            return Unexpected(exp.error());
        }
    }
    return result;
}

/** Build the layout, prepare multipage outputs, and normalize settings.
    The split from loadTestSettings lets the caller run addon-generator
    discovery in between, so the chosen generator's file extension is
    known when paths are computed.
*/
Expected<ResolvedLayout>
buildTestLayout(
    llvm::StringRef filePath,
    LoadedTestSettings loaded,
    llvm::StringRef generatorExtension,
    ReferenceDirectories const& dirs,
    Action action)
{
    bool const dirMultipage = loaded.dirMultipage;
    Config::Settings fileSettings = std::move(loaded.settings);
    bool const hasFileConfig = loaded.hasFileConfig;
    bool const hasTagfileOverride = !fileSettings.tagfile.empty();

    TestLayout layout;
    layout.hasFileConfig = hasFileConfig;

    // The no-op generator has no file extension and produces no output.
    // Run extraction only: normalize the settings and skip all expected
    // output computation and validation.
    if (generatorExtension.empty())
    {
        layout.mode = OutputMode::None;
        if (auto exp = fileSettings.normalize(dirs); !exp)
        {
            return Unexpected(exp.error());
        }
        if (!hasTagfileOverride)
        {
            fileSettings.tagfile.clear();
        }
        return ResolvedLayout{
            std::move(fileSettings),
            std::move(layout)
        };
    }

    layout.expectedSinglePath = pathWithExtension(filePath, generatorExtension).str();
    layout.multipageRoot = pathWithExtension(filePath, "multipage").str();
    layout.multipageFormatRoot = files::appendPath(layout.multipageRoot, generatorExtension);

    if (fileSettings.multipage)
    {
        layout.mode = OutputMode::Multipage;
        layout.tempDir = std::make_unique<ScopedTempDirectory>("mrdocs-multipage");
        if (layout.tempDir->failed())
        {
            return Unexpected(layout.tempDir->error());
        }
        layout.generatedOutputRoot = files::appendPath(layout.tempDir->path(), generatorExtension);
        fileSettings.output = layout.generatedOutputRoot;
        if (!fileSettings.tagfile.empty())
        {
            if (!llvm::sys::path::is_absolute(fileSettings.tagfile))
            {
                fileSettings.tagfile = files::appendPath(
                    layout.generatedOutputRoot, fileSettings.tagfile);
            }
        }
    }
    else
    {
        layout.mode = OutputMode::SinglePage;
    }

    if (auto exp = fileSettings.normalize(dirs); !exp)
    {
        return Unexpected(exp.error());
    }

    // Only generate a tagfile when a test explicitly requests one.
    if (!hasTagfileOverride)
    {
        fileSettings.tagfile.clear();
    }

    auto const singleType = files::getFileType(layout.expectedSinglePath);
    if (!singleType)
    {
        return Unexpected(singleType.error());
    }
    auto const multipageType = files::getFileType(layout.multipageRoot);
    if (!multipageType)
    {
        return Unexpected(multipageType.error());
    }
    auto const multipageFormatType = files::getFileType(layout.multipageFormatRoot);
    if (!multipageFormatType)
    {
        return Unexpected(multipageFormatType.error());
    }

    auto const singleExpectedExists = singleType.value() == files::FileType::regular;
    auto const multipageRootExists = multipageType.value() == files::FileType::directory;
    auto const multipageFormatExists = multipageFormatType.value() == files::FileType::directory;

    if (layout.mode == OutputMode::Multipage)
    {
        if (!layout.hasFileConfig)
        {
            return Unexpected(Error("multipage tests require a per-file mrdocs.yml with multipage: true"));
        }

        if (dirMultipage)
        {
            return Unexpected(Error("multipage defaults must remain disabled at the directory level"));
        }

        if (singleExpectedExists)
        {
            return Unexpected(Error("multipage test cannot have single-page expected outputs"));
        }

        if (singleType.value() == files::FileType::directory)
        {
            return Unexpected(Error("unexpected directory where single-page expectation would be"));
        }

        if (multipageType.value() == files::FileType::regular)
        {
            return Unexpected(Error("multipage snapshot path must be a directory"));
        }

        if (action == Action::test && !multipageFormatExists)
        {
            return Unexpected(Error("missing multipage snapshot for generator"));
        }
    }
    else
    {
        if (multipageRootExists || multipageFormatExists)
        {
            return Unexpected(Error("single-page test cannot have a .multipage snapshot"));
        }
        if (action == Action::test && !singleExpectedExists)
        {
            return Unexpected(Error("missing test file"));
        }
    }

    return ResolvedLayout{
        std::move(fileSettings),
        std::move(layout)
    };
}

} // namespace mrdocs
