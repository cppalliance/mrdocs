//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "test/Options.hpp"
#include "test/SingleFile.hpp"
#include "api/ConfigImpl.hpp"
#include <mrdox/Config.hpp>
#include "api/Support/Debug.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Generators.hpp>
#include <clang/Tooling/StandaloneExecution.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/ThreadPool.h>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace clang {
namespace mrdox {

extern void dumpCommentTypes();
extern void dumpCommentCommands();

using SmallString = llvm::SmallString<0>;

//------------------------------------------------

struct Results
{
    using clock_type = std::chrono::system_clock;

    clock_type::time_point startTime;
    std::atomic<std::size_t> numberOfDirs = 0;
    std::atomic<std::size_t> numberOfFiles = 0;
    std::atomic<std::size_t> numberOfErrors = 0;
    std::atomic<std::size_t> numberOfFailures = 0;
    std::atomic<std::size_t> numberofFilesWritten = 0;

    Results()
        : startTime(clock_type::now())
    {
    }

    // Return the number of milliseconds of elapsed time
    auto
    elapsedMilliseconds() const noexcept
    {
        return std::chrono::duration_cast<
            std::chrono::milliseconds>(
                clock_type::now() - startTime).count();
    }
};

//------------------------------------------------

// We need a different config for each directory
// or file passed on the command line, and thus
// each input path must have a separate Instance.

class Instance
{
    Results& results_;
    std::string extraYaml_;
    Options const& options_;
    std::shared_ptr<Config const> config_;
    Config::WorkGroup wg_;
    Reporter& R_;
    Generator const* xmlGen_;
    Generator const* adocGen_;

    std::shared_ptr<Config const>
    makeConfig(
        llvm::StringRef workingDir);

    llvm::Error
    writeFile(
        llvm::StringRef filePath,
        llvm::StringRef contents);

    llvm::Error
    handleFile(
        llvm::StringRef filePath,
        std::shared_ptr<Config const> const& config);

    llvm::Error
    handleDir(
        llvm::StringRef dirPath);

public:
    Instance(
        Results& results,
        llvm::StringRef extraYaml,
        Options const& options,
        Reporter& R);

    /** Check a single file, or a directory recursively.

        This function checks the specified path
        and blocks until completed.
    */
    llvm::Error
    checkPath(
        llvm::StringRef inputPath);
};

//------------------------------------------------

Instance::
Instance(
    Results& results,
    llvm::StringRef extraYaml,
    Options const& options,
    Reporter& R)
    : results_(results)
    , extraYaml_(extraYaml)
    , options_(options)
    , config_([&extraYaml]
        {
            std::error_code ec;
            auto config = loadConfigString(
                "", extraYaml.str(), ec);
            Assert(! ec);
            return config;
        }())
    , wg_(config_.get())
    , R_(R)
    , xmlGen_(getGenerators().find("xml"))
    , adocGen_(getGenerators().find("adoc"))
{
    Assert(xmlGen_ != nullptr);
    Assert(adocGen_ != nullptr);
}

std::shared_ptr<Config const>
Instance::
makeConfig(
    llvm::StringRef workingDir)
{
    std::string configYaml;
    llvm::raw_string_ostream(configYaml) <<
        "verbose: false\n"
        "source-root: " << workingDir << "\n"
        "with-private: true\n"
        "generator:\n"
        "  xml:\n"
        "    index: false\n"
        "    prolog: true\n";

    std::error_code ec;
    auto config = loadConfigString(
        workingDir, configYaml, 
        ec);
    Assert(! ec);
    return config;
}

llvm::Error
Instance::
writeFile(
    llvm::StringRef filePath,
    llvm::StringRef contents)
{
    std::error_code ec;
    llvm::raw_fd_ostream os(filePath, ec, llvm::sys::fs::OF_None);
    if(ec)
    {
        results_.numberOfErrors++;
        return makeError("raw_fd_ostream returned ", ec);
    }
    os << contents;
    if(os.error())
    {
        results_.numberOfErrors++;
        return makeError("raw_fd_ostream::write returned ", os.error());
    }
    //R_.print("File '", outputPath, "' written.");
    results_.numberofFilesWritten++;
    return llvm::Error::success();
}

llvm::Error
Instance::
handleFile(
    llvm::StringRef filePath,
    std::shared_ptr<Config const> const& config)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    Assert(path::extension(filePath).compare_insensitive(".cpp") == 0);

    results_.numberOfFiles++;

    SmallString dirPath = filePath;
    path::remove_filename(dirPath);

    SmallString outputPath = filePath;
    path::replace_extension(outputPath, xmlGen_->fileExtension());

    // Build Corpus
    std::unique_ptr<Corpus> corpus;
    {
        SingleFile db(dirPath, filePath);
        tooling::StandaloneToolExecutor ex(db, { std::string(filePath) });
        auto result = Corpus::build(ex, config, R_);
        if(R_.error(result, "build Corpus for '", filePath, "'"))
        {
            results_.numberOfErrors++;
            return llvm::Error::success(); // keep going
        }
        corpus = std::move(result.get());
    }

    // Generate XML
    std::string generatedXml;
    if(R_.error(
        xmlGen_->buildOneString(generatedXml, *corpus, R_),
        "build XML string for '", filePath, "'"))
    {
        results_.numberOfErrors++;
        return llvm::Error::success(); // keep going
    }

    if(options_.TestAction == Action::test)
    {
        // Open and load XML comparison file
        std::unique_ptr<llvm::MemoryBuffer> expectedXml;
        {
            auto result = llvm::MemoryBuffer::getFile(outputPath, false);
            if(! result)
            {
                // File could not be loaded
                results_.numberOfErrors++;

                if( result.getError() != std::errc::no_such_file_or_directory)
                {
                    // Some kind of system problem
                    (void)R_.error(result.getError(), "load '", outputPath, "'");
                    return llvm::Error::success(); // keep going
                }

                // File does not exist, so write it
                if(auto err = writeFile(outputPath, generatedXml))
                    return llvm::Error::success();
            }
            else
            {
                expectedXml = std::move(result.get());
            }
        }

        // Compare the generated output with the expected output
        if( expectedXml &&
            generatedXml != expectedXml->getBuffer())
        {
            // The output did not match
            results_.numberOfFailures++;
            R_.print("Failed: '", filePath, "'\n");

            if(options_.badOption.getValue())
            {
                // Write the .bad.xml file
                auto bad = outputPath;
                path::replace_extension(bad, "bad.xml");
                {
                    std::error_code ec;
                    llvm::raw_fd_ostream os(bad, ec, llvm::sys::fs::OF_None);
                    if (ec) {
                        results_.numberOfErrors++;
                        return makeError("raw_fd_ostream returned ", ec);
                    }
                    os << generatedXml;
                }

                auto diff = llvm::sys::findProgramByName("diff");

                if (!diff.getError())
                {
                    path::replace_extension(bad, "xml");
                    std::array<llvm::StringRef, 5u> args {
                        diff.get(), "-u", "--color", bad, outputPath };
                    llvm::sys::ExecuteAndWait(diff.get(), args);
                }

                // Fix the path for the code that follows
                outputPath.pop_back_n(8);
                path::replace_extension(outputPath, "xml");
            }
        }
        else
        {
            // success
        }
    }
    else if(options_.TestAction == Action::refresh)
    {
        // Refresh the expected output file
        if(auto err = writeFile(outputPath, generatedXml))
        {
            results_.numberOfErrors++;
            return llvm::Error::success();
        }
    }

    // Write Asciidoc if requested
    if(options_.adocOption.getValue())
    {
        path::replace_extension(outputPath, adocGen_->fileExtension());
        if(R_.error(
            adocGen_->buildOne(outputPath.str(), *corpus, R_),
            "write '", outputPath, "'"))
            return llvm::Error::success(); // keep going
    }

    return llvm::Error::success();
}

llvm::Error
Instance::
handleDir(
    llvm::StringRef dirPath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    results_.numberOfDirs++;

    // Set up directory iterator
    std::error_code ec;
    fs::directory_iterator iter(dirPath, ec, false);
    if(ec)
        return makeError("directory_iterator returned ", ec);
    fs::directory_iterator const end{};

    auto const config = makeConfig(dirPath);

    while(iter != end)
    {
        if(iter->type() == fs::file_type::directory_file)
        {
            if(auto err = handleDir(iter->path()))
                return err;
        }
        else if(
            iter->type() == fs::file_type::regular_file &&
            path::extension(iter->path()).equals_insensitive(".cpp"))
        {
            wg_.post(
                [this, config, filePath = SmallString(iter->path())]
                {
                    handleFile(filePath, config).operator bool();
                });
        }
        else
        {
            // we don't handle this type
        }
        iter.increment(ec);
        if(ec)
            return makeError("directory_iterator returned ", ec);
    }
    return llvm::Error::success();
}

llvm::Error
Instance::
checkPath(
    llvm::StringRef inputPath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // See if inputPath references a file or directory
    fs::file_status fileStatus;
    if(auto ec = fs::status(inputPath, fileStatus))
    {
        results_.numberOfErrors++;
        return makeError("fs::status returned '", ec, "'");
    }

    if(fileStatus.type() == fs::file_type::regular_file)
    {
        auto const ext = path::extension(inputPath);
        if(! ext.equals_insensitive(".cpp"))
            return makeError("expected a .cpp file");

        // Calculate the workingDir
        SmallString workingDir(inputPath);
        path::remove_filename(workingDir);
        path::remove_dots(workingDir, true);

        auto config = makeConfig(workingDir);
        auto err = handleFile(inputPath, config);
        wg_.wait();
        return err;
    }

    if(fileStatus.type() == fs::file_type::directory_file)
    {
        // Iterate this directory and all its children
        SmallString dirPath(inputPath);
        path::remove_dots(dirPath, true);
        auto err = handleDir(dirPath);
        wg_.wait();
        return err;
    }

    return makeError("wrong fs::file_type=", static_cast<int>(fileStatus.type()));
}

//------------------------------------------------

static Options options;

} // mrdox
} // clang

int
main(int argc, const char** argv)
{
    using namespace clang::mrdox;

    // VFALCO This is crashing now on exit...
    //llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    debugEnableHeapChecking();

    Reporter R;

    {
        std::string errorString;
        llvm::raw_string_ostream os(errorString);
        if(! llvm::cl::ParseCommandLineOptions(
            argc, argv, options.Overview, &os, nullptr))
        {
            llvm::errs() << errorString;
            return EXIT_FAILURE;
        }
    }

    std::string extraYaml;
    llvm::raw_string_ostream(extraYaml) <<
        "concurrency: 1\n";
    Results results;
    for(auto const& inputPath : options.InputPaths)
    {
        Instance instance(results, extraYaml, options, R);
        if(auto err = instance.checkPath(inputPath))
            if(R.error(err, "check path '", inputPath, "'"))
                break;
    }

    auto& os = debug_outs();
    if(results.numberofFilesWritten > 0)
        os <<
            results.numberofFilesWritten << " files written\n";
    os <<
        "Checked " <<
        results.numberOfFiles << " files (" <<
        results.numberOfDirs << " dirs)";
    if( results.numberOfErrors > 0 ||
        results.numberOfFailures > 0)
    {
        if( results.numberOfErrors > 0)
        {
            os <<
                ", with " <<
                results.numberOfErrors << " errors";
            if(results.numberOfFailures > 0)
                os <<
                    " and " << results.numberOfFailures <<
                    " failures";
        }
        else
        {
            os <<
                ", with " <<
                results.numberOfFailures <<
                " failures";
        }
    }
    auto milli = results.elapsedMilliseconds();
    if(milli < 10000)
        os <<
            " in " << milli << " milliseconds\n";
#if 0
    else if(milli < 10000)
        os <<
            " in " << std::setprecision(1) <<
            double(milli) / 1000 << " seconds\n";
#endif
    else
        os <<
            " in " << ((milli + 500) / 1000) <<
            " seconds\n";

    if( results.numberOfFailures > 0 ||
        results.numberOfErrors > 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
