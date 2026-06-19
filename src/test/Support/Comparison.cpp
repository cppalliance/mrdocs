//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#include "Comparison.hpp"
#include "TextNormalization.hpp"
#include "../TestRunner.hpp"
#include "../TestArgs.hpp"
#include <test_suite/diff.hpp>
#include <lib/Support/Generator.hpp>
#include <lib/Support/Path.hpp>
#include <lib/Support/Report.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error.hpp>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <filesystem>
#include <map>
#include <utility>
#include <vector>

namespace mrdocs::test_support {

namespace {

/** Collect the regular files under `root` as (relative name -> path).

    Hidden files (a dot-prefixed segment, e.g. .DS_Store) are skipped so OS
    artifacts stay out of comparisons.
*/
bool
collectFiles(
    std::string const& root,
    std::map<std::string, std::filesystem::path>& files)
{
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::path const rootPath(root);
    for (fs::recursive_directory_iterator it(rootPath, ec), end; it != end; it.increment(ec))
    {
        if (ec)
        {
            report::error("{}: \"{}\"", Error(ec), root);
            return false;
        }

        if (!it->is_regular_file())
            continue;

        auto rel = fs::relative(it->path(), rootPath, ec);
        if (ec)
        {
            report::error("{}: \"{}\"", Error(ec), it->path().string());
            return false;
        }

        bool hidden = false;
        for (auto const& part : rel)
        {
            auto const name = part.string();
            if (!name.empty() && name.front() == '.')
            {
                hidden = true;
                break;
            }
        }
        if (hidden)
            continue;

        files.emplace(rel.generic_string(), it->path());
    }
    return true;
}

bool
copyDirectoryTree(
    std::string const& from,
    std::string const& to)
{
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::remove_all(to, ec);
    ec.clear();
    fs::create_directories(fs::path(to).parent_path(), ec);
    fs::create_directories(to, ec);
    fs::copy(from, to,
        fs::copy_options::recursive |
        fs::copy_options::overwrite_existing,
        ec);
    if (ec)
    {
        report::error("{}: \"{}\"", Error(ec), to);
        return false;
    }
    return true;
}

} // (anon)

Expected<void>
generateAndCompareOutput(CompareArgs const& args)
{
    namespace fs = std::filesystem;
    namespace path = llvm::sys::path;

    // The generator's output directory: where it writes, and where we read
    // it back. Single-page vs multipage follows the config that drove the
    // build; the expected-fixture paths derive from the input plus the
    // generator's extension.
    std::string const outputDir = getGeneratorOutputPath(args.gen, args.corpus);
    bool const singlePage = !args.corpus.config->multipage;
    std::string const expectedSinglePath =
        files::withExtension(args.filePath, args.gen.fileExtension());
    std::string const multipageFormatRoot = files::appendPath(
        files::withExtension(args.filePath, "multipage"),
        args.gen.fileExtension());

    // Build into a clean output directory. The generator writes its files
    // there; nothing past this point cares whether that is one file or a
    // whole tree.
    std::error_code ec;
    fs::remove_all(outputDir, ec);
    ec.clear();
    fs::create_directories(outputDir, ec);
    if (ec)
    {
        return Unexpected(Error(ec));
    }

    report::debug("Generating documentation", args.filePath);
    if (auto exp = args.gen.build(args.corpus); !exp)
    {
        return Unexpected(exp.error());
    }

    // Gather the expected and generated files as (relative name -> path)
    // pairs. Single-page maps only the primary document (ignoring any
    // stylesheet or tagfile written beside it); multipage maps the whole
    // tree on each side. From here on the two modes share one code path.
    std::map<std::string, fs::path> expectedFiles;
    std::map<std::string, fs::path> generatedFiles;
    std::string fixturePath;   // expected file or snapshot dir, for messages
    bool expectedExists = false;

    if (singlePage)
    {
        MRDOCS_TRY(std::string const primary,
            getSinglePageFullPath(
                outputDir, args.gen.fileExtension()));
        std::string const key{files::getFileName(expectedSinglePath)};
        generatedFiles.emplace(key, fs::path(primary));
        fixturePath = expectedSinglePath;
        expectedExists = files::isRegularFile(expectedSinglePath);
        if (expectedExists)
        {
            expectedFiles.emplace(key, fs::path(expectedSinglePath));
        }
    }
    else
    {
        fixturePath = multipageFormatRoot;
        expectedExists = files::isDirectory(multipageFormatRoot);
        if (!collectFiles(outputDir, generatedFiles))
        {
            return Unexpected(Error("failed to read generated output"));
        }
        if (expectedExists &&
            !collectFiles(multipageFormatRoot, expectedFiles))
        {
            return Unexpected(Error("failed to read expected snapshot"));
        }
    }

    // Persist the freshly generated output as the fixture: copy the single
    // primary document over the expected file, or mirror the whole tree.
    auto writeFixture = [&]() -> bool
    {
        if (singlePage)
        {
            std::error_code copyEc;
            fs::create_directories(
                fs::path(expectedSinglePath).parent_path(), copyEc);
            copyEc.clear();
            fs::copy_file(generatedFiles.begin()->second,
                expectedSinglePath,
                fs::copy_options::overwrite_existing, copyEc);
            if (copyEc)
            {
                report::error("{}: \"{}\"", Error(copyEc), expectedSinglePath);
                return false;
            }
            return true;
        }
        return copyDirectoryTree(
            outputDir, multipageFormatRoot);
    };

    // A missing fixture is an error under --action=test, otherwise it is
    // created from the generated output.
    if (!expectedExists)
    {
        if (args.action == Action::test)
        {
            return Unexpected(Error(singlePage
                ? "missing test file"
                : "missing multipage snapshot for generator"));
        }
        if (writeFixture())
        {
            report::info("\"{}\" created", fixturePath);
            ++args.results.expectedDocsWritten;
        }
        return {};
    }

    // Diff the two file sets.
    std::vector<std::string> missing;
    std::vector<std::string> unexpected;
    std::vector<std::pair<std::string, test_suite::DiffStringsResult>> changed;

    for (auto const& [rel, expectedPath] : expectedFiles)
    {
        if (!generatedFiles.contains(rel))
            missing.push_back(rel);
    }

    for (auto const& [rel, generatedPath] : generatedFiles)
    {
        auto it = expectedFiles.find(rel);
        if (it == expectedFiles.end())
        {
            unexpected.push_back(rel);
            continue;
        }

        auto expectedBuf = llvm::MemoryBuffer::getFile(it->second.string(), false, true, true);
        if (!expectedBuf)
        {
            report::error("{}: \"{}\"", expectedBuf.getError(), it->second.string());
            continue;
        }
        auto generatedBuf = llvm::MemoryBuffer::getFile(generatedPath.string(), false, true, true);
        if (!generatedBuf)
        {
            report::error("{}: \"{}\"", generatedBuf.getError(), generatedPath.string());
            continue;
        }

        auto const format = guessOutputFormat(rel);
        auto const expectedNormalized = normalizeForComparison(
            expectedBuf.get()->getBuffer(), format);
        auto const generatedNormalized = normalizeForComparison(
            generatedBuf.get()->getBuffer(), format);
        if (expectedNormalized != generatedNormalized)
        {
            changed.emplace_back(rel, test_suite::diffStrings(
                expectedNormalized, generatedNormalized));
        }
    }

    bool const hasDiff =
        !missing.empty() || !unexpected.empty() || !changed.empty();

    if (args.action == Action::update)
    {
        if (!hasDiff && !args.forceUpdate)
        {
            report::info("\"{}\" unchanged", fixturePath);
            ++args.results.expectedDocsMatching;
            return {};
        }
        if (writeFixture())
        {
            report::info("\"{}\" {}", fixturePath, hasDiff ? "updated" : "rewritten");
            ++args.results.expectedDocsWritten;
        }
        return {};
    }

    if (!hasDiff)
    {
        report::info("\"{}\" passed", args.filePath);
        ++args.results.expectedDocsMatching;
        return {};
    }

    // action == test or create: report the differences. The fixture is left
    // untouched (create only writes a fixture that was missing).
    for (auto const& rel : missing)
    {
        report::error("{}: \"{}\"", Error("missing expected file"),
            files::appendPath(fixturePath, rel));
    }
    for (auto const& rel : unexpected)
    {
        report::error("{}: \"{}\"", Error("unexpected generated file"),
            files::appendPath(outputDir, rel));
    }
    for (auto const& [rel, diff] : changed)
    {
        report::error("{}: \"{}\"", Error("Incorrect results"),
            singlePage ? fixturePath : files::appendPath(fixturePath, rel));
        report::error("{} lines added", diff.added);
        report::error("{} lines removed", diff.removed);
        report::error("Diff:\n{}", diff.diff);
    }

    // Write the generated document beside the fixture for inspection
    // (single-page only, matching the historical --bad behavior).
    if (args.writeBad && singlePage && !changed.empty())
    {
        SmallPathString badPath(expectedSinglePath);
        path::replace_extension(
            badPath, llvm::Twine("bad.").concat(args.gen.fileExtension()));
        std::error_code copyEc;
        fs::copy_file(generatedFiles.begin()->second, std::string(badPath),
            fs::copy_options::overwrite_existing, copyEc);
        if (copyEc)
        {
            return Unexpected(Error(copyEc));
        }
        report::info("\"{}\" written", badPath);
    }

    return {};
}

} // namespace mrdocs::test_support
