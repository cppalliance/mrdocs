//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Corpus.hpp>
#include <mrdox/Reporter.hpp>
#include <mrdox/Generator.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/StandaloneExecution.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/ThreadPool.h>
#include <string_view>
#include <vector>

// Each test comes as a pair of files.
// A .cpp file containing valid declarations,
// and a .xml file containing the expected output
// of the XML generator, which must match exactly.

namespace clang {
namespace mrdox {

//------------------------------------------------

/** Compilation database for a single .cpp file.
*/
class SingleFile
    : public tooling::CompilationDatabase
{
    std::vector<tooling::CompileCommand> cc_;

public:
    SingleFile(
        llvm::StringRef dir,
        llvm::StringRef file,
        llvm::StringRef output)
    {
        std::vector<std::string> cmds;
        cmds.emplace_back("clang");
        cmds.emplace_back(file);
        cc_.emplace_back(
            dir,
            file,
            std::move(cmds),
            output);
        cc_.back().Heuristic = "unit test";
    }

    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override
    {
        if(! FilePath.equals(cc_.front().Filename))
            return {};
        return { cc_.front() };
    }

    std::vector<std::string>
    getAllFiles() const override
    {
        return { cc_.front().Filename };
    }

    std::vector<tooling::CompileCommand>
    getAllCompileCommands() const override
    {
        return { cc_.front() };
    }
};

//------------------------------------------------

struct Tester
{
    Config const& config_;
    std::unique_ptr<Generator> xmlGen;
    std::unique_ptr<Generator> adocGen;
    Reporter& R_;

    explicit
    Tester(
        Config const& config,
        Reporter &R)
        : config_(config)
        , xmlGen(makeXMLGenerator())
        , adocGen(makeAsciidocGenerator())
        , R_(R)
    {
    }

    bool
    testDirectory(
        llvm::SmallString<340> dirPath,
        llvm::ThreadPool& threadPool)
    {
        namespace fs = llvm::sys::fs;
        namespace path = llvm::sys::path;

        std::error_code ec;
        llvm::SmallString<32> outputPath;
        path::remove_dots(dirPath, true);
        fs::directory_iterator const end{};

        fs::directory_iterator iter(dirPath, ec, false);
        if(ec)
        {
            R_.failed("visitDirectory", ec);
            return false;
        }

        while(iter != end)
        {
            if(iter->type() == fs::file_type::directory_file)
            {
                if(! testDirectory(llvm::StringRef(iter->path()), threadPool))
                    return false;
            }
            else if(
                iter->type() == fs::file_type::regular_file &&
                path::extension(iter->path()).equals_insensitive(".cpp"))
            {
                outputPath = iter->path();
                path::replace_extension(outputPath, "xml");
                threadPool.async([this,
                    dirPath,
                    inputPath = llvm::SmallString<0>(iter->path()),
                    outputPath = llvm::SmallString<340>(outputPath)]() mutable
                    {
                        SingleFile db(dirPath, inputPath, outputPath);
                        tooling::StandaloneToolExecutor ex(db, { std::string(inputPath) });
                        auto corpus = Corpus::build(ex, config_, R_);
                        if(corpus)
                            performTest(*corpus, inputPath, outputPath);
                    });
            }
            else
            {
                // we don't handle this type
            }
            iter.increment(ec);
            if(ec)
            {
                R_.failed("fs::directory_iterator::increment", ec);
                return false;
            }
        }
        return true;
    }

    void
    performTest(
        Corpus& corpus,
        llvm::StringRef inputPath,
        llvm::SmallVectorImpl<char>& outputPathStr)
    {
        namespace fs = llvm::sys::fs;
        namespace path = llvm::sys::path;

        llvm::StringRef outputPath;
        outputPath = { outputPathStr.data(), outputPathStr.size() };

        std::string xmlString;
        if(! xmlGen->buildString(xmlString, corpus, config_, R_))
        {
            R_.testFailed();
            return;
        }
        std::error_code ec;
        fs::file_status stat;
        ec = fs::status(outputPathStr, stat, false);
        if(ec == std::errc::no_such_file_or_directory)
        {
            // create the xml file and write to it
            llvm::raw_fd_ostream os(outputPath, ec, llvm::sys::fs::OF_None);
            if(! ec)
            {
                os << xmlString;
            }
            else
            {
                llvm::errs() <<
                    "Writing \"" << outputPath << "\" failed: " <<
                    ec.message() << "\n";
                R_.testFailed();
            }
        }
        else if(ec)
        {
            R_.failed("fs::status", ec);
            return;
        }
        if(stat.type() != fs::file_type::regular_file)
        {
            R_.failed("'%s' is not a regular file", outputPath);
            return;
        }
        auto bufferResult = llvm::MemoryBuffer::getFile(outputPath, false);
        if(! bufferResult)
        {
            R_.failed("MemoryBuffer::getFile", bufferResult);
            return;
        }
        std::string_view got(bufferResult->get()->getBuffer());
        if(xmlString != bufferResult->get()->getBuffer())
        {
            R_.errs(
                "File: \"", inputPath, "\" failed.\n",
                "Expected:\n",
                bufferResult->get()->getBuffer(), "\n",
                "Got:\n", xmlString, "\n");
            R_.testFailed();
        }

        if(adocGen)
        {
            path::replace_extension(outputPathStr, adocGen->extension());
            outputPath = { outputPathStr.data(), outputPathStr.size() };
            adocGen->buildOne(outputPath, corpus, config_, R_);
        }
    }
};

//------------------------------------------------

void
testMain(
    int argc, const char* const* argv,
    Reporter& R)
{
    namespace path = llvm::sys::path;

    // Each command line argument is processed
    // as a directory which will be iterated
    // recursively for tests.
    for(int i = 1; i < argc; ++i)
    {
        Config config;
        {
            llvm::SmallString<0> s(argv[i]);
            path::remove_dots(s, true);
            config.includePaths.emplace_back(std::move(s));
        }
        // Use the include path for the configPath
        config.configPath = config.includePaths[0];

        // We need a different config for each directory
        // passed on the command line, and thus each must
        // also have a separate Tester.
        Tester tester(config, R);
        llvm::StringRef s(argv[i]);
        llvm::SmallString<340> dirPath(s);
        path::remove_dots(dirPath, true);
        llvm::ThreadPool threadPool(
            llvm::hardware_concurrency(
                tooling::ExecutorConcurrency));
        if(! tester.testDirectory(llvm::StringRef(argv[i]), threadPool))
            break;
        threadPool.wait();
    }
}

} // mrdox
} // clang

int
main(int argc, const char** argv)
{
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    clang::mrdox::Reporter R;
    clang::mrdox::testMain(argc, argv, R);
    return R.getExitCode();
}
