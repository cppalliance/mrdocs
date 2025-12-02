//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#include "Comparison.hpp"
#include "TextNormalization.hpp"
#include "../TestRunner.hpp"
#include <test_suite/diff.hpp>
#include <lib/Gen/hbs/HandlebarsGenerator.hpp>
#include <lib/Support/Path.hpp>
#include <lib/Support/Report.hpp>
#include <mrdocs/Support/Error.hpp>
#include "../TestArgs.hpp"
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <sstream>

namespace mrdocs::test_support {

namespace {

/** Strip cwd prefix to make error paths shorter. */
std::string
relativeToCwd(
    ReferenceDirectories const& dirs,
    std::string_view path)
{
    std::string_view trimmed = path;
    if (trimmed.starts_with(dirs.cwd))
    {
        trimmed.remove_prefix(dirs.cwd.size());
        if (trimmed.starts_with("\\") || trimmed.starts_with("/"))
        {
            trimmed.remove_prefix(1);
        }
    }
    return std::string(trimmed);
}

bool
collectFiles(
    std::string const& root,
    std::unordered_map<std::string, std::filesystem::path>& files)
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

        // Ignore hidden files (e.g. .DS_Store) to keep OS artifacts out of comparisons.
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

Expected<void>
writeFile(
    llvm::StringRef filePath,
    llvm::StringRef contents)
{
    std::error_code ec;
    llvm::raw_fd_ostream os(
        filePath, ec, llvm::sys::fs::OF_None);
    MRDOCS_CHECK(!ec, ec);
    os << contents;
    MRDOCS_CHECK(!os.has_error(), os.error());
    return {};
}

} // (anon)

Expected<void>
compareSinglePage(SinglePageArgs const& args)
{
    namespace path = llvm::sys::path;

    report::debug("Generating documentation", args.filePath);
    std::string generatedDocs;
    if (auto exp = args.gen.buildOneString(generatedDocs, args.corpus); !exp)
    {
        return Unexpected(exp.error());
    }

    auto const format = guessOutputFormat(args.layout.expectedSinglePath);
    std::string normalizedGenerated = normalizeForComparison(
        generatedDocs, format);

    // Generate tagfile for Handlebars
    if (auto hbsGen = dynamic_cast<hbs::HandlebarsGenerator const*>(&args.gen))
    {
        report::debug("Generating tagfile", args.filePath);
        std::stringstream ss;
        if (auto exp = hbsGen->buildTagfile(ss, args.corpus); !exp)
        {
            return Unexpected(exp.error());
        }
    }

    // Get expected documentation if it exists
    std::unique_ptr<llvm::MemoryBuffer> expectedDocsBuf;
    {
        auto fileResult = llvm::MemoryBuffer::getFile(args.layout.expectedSinglePath, false, true, true);
        if (fileResult)
        {
            expectedDocsBuf = std::move(fileResult.get());
        } else if (fileResult.getError() != std::errc::no_such_file_or_directory)
        {
            return Unexpected(Error(fileResult.getError()));
        }
    }

    // If no expected documentation file
    if(!expectedDocsBuf)
    {
        if(args.action == Action::test)
        {
            return Unexpected(Error("missing test file"));
        }

        if(args.action == Action::create ||
           args.action == Action::update)
        {
            if(auto exp = writeFile(args.layout.expectedSinglePath, generatedDocs); !exp)
            {
                return Unexpected(exp.error());
            }
            report::info("\"{}\" created", args.layout.expectedSinglePath);
            ++args.results.expectedDocsWritten;
            return {};
        }
    }

    std::string const expectedDocs = normalizeForComparison(
        expectedDocsBuf->getBuffer(), format);
    if (normalizedGenerated == expectedDocs)
    {
        report::info("\"{}\" passed", args.filePath);
        ++args.results.expectedDocsMatching;
        return {};
    }

    if(
        args.action == Action::test ||
        args.action == Action::create)
    {
        auto relPath = relativeToCwd(args.dirs, args.filePath);
        report::error("{}: \"{}\"",
            Error("Incorrect results"), relPath);
        auto res = test_suite::diffStrings(expectedDocs, normalizedGenerated);
        report::error("{} lines added", res.added);
        report::error("{} lines removed", res.removed);

        report::error("Diff:\n{}", res.diff);

        if(args.writeBad)
        {
            SmallPathString badPath(args.layout.expectedSinglePath);
            path::replace_extension(badPath, llvm::Twine("bad.").concat(args.gen.fileExtension()));
            if (auto exp = writeFile(badPath, generatedDocs); !exp)
            {
                return Unexpected(exp.error());
            }
            report::info("\"{}\" written", badPath);
            report::error("Bad file diff (internal):\n{}", res.diff);
        }
    }
    else if(args.action == Action::update)
    {
        bool const differs = expectedDocs != normalizedGenerated;
        if (!differs && !args.forceUpdate)
        {
            report::info("\"{}\" unchanged", args.layout.expectedSinglePath);
            ++args.results.expectedDocsMatching;
            return {};
        }

        if (auto exp = writeFile(args.layout.expectedSinglePath, generatedDocs); !exp)
        {
            return Unexpected(exp.error());
        }
        report::info("\"{}\" {}", args.layout.expectedSinglePath, differs ? "updated" : "rewritten");
        ++args.results.expectedDocsWritten;
    }

    return {};
}

Expected<void>
compareMultipage(MultipageArgs const& args)
{
    if (args.layout.generatedOutputRoot.empty())
    {
        return Unexpected(Error("missing output directory for multipage run"));
    }

    // Prepare output directory
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::remove_all(args.layout.generatedOutputRoot, ec);
    ec.clear();
    fs::create_directories(args.layout.generatedOutputRoot, ec);
    if (ec)
    {
        return Unexpected(Error(ec));
    }

    report::debug("Generating multipage documentation", args.layout.generatedOutputRoot);
    if (auto exp = args.gen.build(args.layout.generatedOutputRoot, args.corpus); !exp)
    {
        return Unexpected(exp.error());
    }

    auto expectedType = files::getFileType(args.layout.multipageFormatRoot);
    if (!expectedType)
    {
        return Unexpected(expectedType.error());
    }
    bool const expectedExists =
        expectedType.value() == files::FileType::directory;

    if (!expectedExists)
    {
        if (args.action == Action::test)
        {
            return Unexpected(Error("missing multipage snapshot for generator"));
        }

        if (copyDirectoryTree(args.layout.generatedOutputRoot, args.layout.multipageFormatRoot))
        {
            report::info("\"{}\" created", args.layout.multipageFormatRoot);
            ++args.results.expectedDocsWritten;
        }
        return {};
    }

    std::unordered_map<std::string, std::filesystem::path> expectedFiles;
    std::unordered_map<std::string, std::filesystem::path> generatedFiles;
    if (!collectFiles(args.layout.multipageFormatRoot, expectedFiles))
        return Unexpected(Error("failed to read expected multipage snapshot"));
    if (!collectFiles(args.layout.generatedOutputRoot, generatedFiles))
        return Unexpected(Error("failed to read generated multipage output"));

    std::vector<std::string> missing;
    std::vector<std::string> unexpected;
    std::vector<std::pair<std::string, test_suite::DiffStringsResult>> changed;

    for (auto const& [rel, path] : expectedFiles)
    {
        if (!generatedFiles.contains(rel))
            missing.push_back(rel);
    }

    for (auto const& [rel, path] : generatedFiles)
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
        auto generatedBuf = llvm::MemoryBuffer::getFile(path.string(), false, true, true);
        if (!generatedBuf)
        {
            report::error("{}: \"{}\"", generatedBuf.getError(), path.string());
            continue;
        }

        auto const format = guessOutputFormat(rel);
        auto expectedNormalized = normalizeForComparison(
            expectedBuf.get()->getBuffer(), format);
        auto generatedNormalized = normalizeForComparison(
            generatedBuf.get()->getBuffer(), format);
        if (expectedNormalized != generatedNormalized)
        {
            changed.emplace_back(rel, test_suite::diffStrings(
                expectedNormalized, generatedNormalized));
        }
    }

    bool const hasDiff = !missing.empty() || !unexpected.empty() || !changed.empty();

    if (args.action == Action::update)
    {
        if (!hasDiff && !args.forceUpdate)
        {
            report::info("\"{}\" unchanged", args.layout.multipageFormatRoot);
            ++args.results.expectedDocsMatching;
            return {};
        }

        if (copyDirectoryTree(args.layout.generatedOutputRoot, args.layout.multipageFormatRoot))
        {
            report::info("\"{}\" {}", args.layout.multipageFormatRoot, hasDiff ? "updated" : "rewritten");
            ++args.results.expectedDocsWritten;
        }
        return {};
    }

    if (!hasDiff)
    {
        ++args.results.expectedDocsMatching;
        return {};
    }

    for (auto const& rel : missing)
    {
        report::error("{}: \"{}\"",
            Error("missing expected file"),
            files::appendPath(args.layout.multipageFormatRoot, rel));
    }
    for (auto const& rel : unexpected)
    {
        report::error("{}: \"{}\"",
            Error("unexpected generated file"),
            files::appendPath(args.layout.generatedOutputRoot, rel));
    }
    for (auto const& [rel, diff] : changed)
    {
        report::error("{}: \"{}\"",
            Error("Incorrect results"),
            files::appendPath(args.layout.multipageFormatRoot, rel));
        report::error("{} lines added", diff.added);
        report::error("{} lines removed", diff.removed);
        report::error("Diff:\n{}", diff.diff);
    }

    return {};
}

} // namespace mrdocs::test_support
