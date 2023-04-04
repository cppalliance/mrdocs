//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "ClangDoc.h"
#include "Mapper.h"
#include "Representation.h"
#include <mrdox/Config.hpp>
#include <mrdox/XML.hpp>
#include <clang/tooling/CompilationDatabase.h>
#include <clang/tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <atomic>
#include <cstdlib>

// VFALCO GARBAGE
extern void force_xml_generator_linkage();

// Each test comes as a pair of files.
// A .cpp file containing valid declarations,
// and a .xml file containing the expected output
// of the XML generator, which must match exactly.

namespace clang {
namespace mrdox {

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

//------------------------------------------------
//
// Generally Helpful Utilties
//
//------------------------------------------------

/** Used to check and report errors uniformly.
*/
struct Reporter
{
    bool failed = false;

    bool
    success(
        llvm::StringRef what,
        std::error_code const& ec)
    {
        if(! ec)
            return true;
        llvm::errs() <<
            what << ": " << ec.message() << "\n";
        failed = true;
        return false;
    }

    bool
    success(
        llvm::StringRef what,
        llvm::Error& err)
    {
        if(! err)
            return true;
        llvm::errs() <<
            what << ": " << toString(std::move(err)) << "\n";
        failed = true;
        return false;
    }
};

/** Return command line arguments as a vector of strings.
*/
std::vector<std::string>
makeVectorOfArgs(int argc, const char** argv)
{
    std::vector<std::string> result;
    result.reserve(argc);
    for(int i = 0; i < argc; ++i)
        result.push_back(argv[i]);
    return result;
}

/** Return an executor from a vector of arguments.
*/
llvm::Expected<std::unique_ptr<ToolExecutor>>
createExecutor(
    std::vector<std::string> const& args,
    llvm::cl::OptionCategory& category,
    char const* overview)
{
    std::vector<const char*> argv;
    argv.reserve(args.size());
    for(auto const& arg : args)
        argv.push_back(arg.data());
    int argc = static_cast<int>(argv.size());
    return clang::tooling::createExecutorFromCommandLineArgs(
        argc, argv.data(), category, overview);
}

//------------------------------------------------

/** Compilation database where files come in pairs of C++ and XML.
*/
class TestCompilationDatabase
    : public clang::tooling::CompilationDatabase
{
    std::vector<CompileCommand> cc_;

public:
    TestCompilationDatabase() = default;

    bool
    addDirectory(
        llvm::StringRef path,
        Reporter& R)
    {
        std::error_code ec;
        llvm::SmallString<256> dir(path);
        path::remove_dots(dir, true);
        fs::directory_iterator const end{};
        fs::directory_iterator iter(dir, ec, false);
        if(! R.success("addDirectory", ec))
            return false;
        while(iter != end)
        {
            if(iter->type() == fs::file_type::directory_file)
            {
                addDirectory(iter->path(), R);
            }
            else if(
                iter->type() == fs::file_type::regular_file &&
                path::extension(iter->path()).equals_insensitive(".cpp"))
            {
                llvm::SmallString<256> output(iter->path());
                path::replace_extension(output, "xml");
                std::vector<std::string> commandLine = {
                    "clang",
                    iter->path()
                };
                cc_.emplace_back(
                    dir,
                    iter->path(),
                    std::move(commandLine),
                    output);
                cc_.back().Heuristic = "unit test";

            }
            else
            {
                // we don't handle this type
            }
            iter.increment(ec);
            if(! R.success("increment", ec))
                return false;
        }
        return true;
    }

    std::vector<CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override
    {
        std::vector<CompileCommand> result;
        for(auto const& cc : cc_)
            if(FilePath.equals(cc.Filename))
                result.push_back(cc);
        return result;
    }

    virtual
    std::vector<std::string>
    getAllFiles() const override
    {
        std::vector<std::string> result;
        result.reserve(cc_.size());
        for(auto const& cc : cc_)
            result.push_back(cc.Filename);
        return result;
    }

    std::vector<CompileCommand>
    getAllCompileCommands() const override
    {
        return cc_;
    }
};

//------------------------------------------------

static
const char* toolOverview =
R"(Runs tests on input files and checks the results,

Example:
    $ mrdox_tests *( DIR | FILE.cpp )
)";

static
llvm::cl::extrahelp
commonHelp(
    tooling::CommonOptionsParser::HelpMessage);

static
llvm::cl::OptionCategory
toolCategory("mrdox_tests options");

//------------------------------------------------

int
testMain(int argc, const char** argv)
{
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    // VFALCO GARBAGE
    force_xml_generator_linkage();

    Reporter R;

    TestCompilationDatabase db;
    for(int i = 1; i < argc; ++i)
        db.addDirectory(argv[i], R);

    auto args = makeVectorOfArgs(argc, argv);
    args.push_back("--executor=all-TUs");
    std::unique_ptr<ToolExecutor> executor;
    if(llvm::Error err = createExecutor(
        args, toolCategory, toolOverview).moveInto(executor))
    {
        return EXIT_FAILURE;
    }

    std::vector<std::string> Args;
    for(auto i = 0; i < argc; ++i)
        Args.push_back(argv[i]);

    Config cfg;
    if(llvm::Error err = setupContext(cfg, argc, argv))
    {
        llvm::errs() << "test failure: " << err << "\n";
        return EXIT_FAILURE;
    }

    std::string xml;
    for(int i = 1; i < argc; ++i)
    {
        std::error_code ec;
        llvm::SmallString<256> dir(argv[i]);
        path::remove_dots(dir, true);
        fs::recursive_directory_iterator const end{};
        fs::recursive_directory_iterator iter(dir, ec, false);
        if(ec)
        {
            llvm::errs() <<
                ec.message() << ": \"" <<
                dir << "\"\n";
            return EXIT_FAILURE;
        }
        while(iter != end)
        {
            {
                auto const& name = iter->path();
                llvm::SmallString<128> cppPath(name);
                llvm::SmallString<128> xmlPath(name);
                path::remove_dots(cppPath, true);
                path::remove_dots(xmlPath, true);
                if(path::extension(name).equals_insensitive(".cpp"))
                {
                    if(! fs::is_regular_file(name))
                    {
                        llvm::errs() <<
                            "invalid file: \"" << name << "\"\n";
                        return EXIT_FAILURE;
                    }
                    path::replace_extension(xmlPath, "xml");
                    if( ! fs::exists(xmlPath) ||
                        ! fs::is_regular_file(xmlPath))
                    {
                        llvm::errs() <<
                            "missing or invalid file: \"" << xmlPath << "\"\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(path::extension(name).equals_insensitive(".xml"))
                {
                    path::replace_extension(cppPath, "cpp");
                    if( ! fs::exists(cppPath) ||
                        ! fs::is_regular_file(cppPath))
                    {
                        llvm::errs() <<
                            "missing or invalid file: \"" << cppPath << "\"\n";
                        return EXIT_FAILURE;
                    }

                    // don't process the same test twice
                    goto loop;
                }
                else
                {
                    // not .cpp or .xml
                    goto loop;
                }

                llvm::StringRef cppCode;
                auto cppResult = llvm::MemoryBuffer::getFile(cppPath, true);
                if(! cppResult)
                {
                    llvm::errs() <<
                        cppResult.getError().message() << ": \"" << xmlPath << "\"\n";
                    return EXIT_FAILURE;
                }
                cppCode = cppResult->get()->getBuffer();

                llvm::StringRef expectedXml;
                auto xmlResult = llvm::MemoryBuffer::getFile(xmlPath, true);
                if(! xmlResult)
                {
                    llvm::errs() <<
                        xmlResult.getError().message() << ": \"" << xmlPath << "\"\n";
                    return EXIT_FAILURE;
                }
                expectedXml = xmlResult->get()->getBuffer();

                if(! renderCodeAsXML(xml, cppCode, cfg))
                    return EXIT_FAILURE;

                if(xml != expectedXml)
                {
                    R.failed = true;
                    llvm::errs() <<
                        "Failed: \"" << xmlPath << "\", got\n" <<
                        xml;
                }
            }
        loop:
            iter.increment(ec);
        }
    }

    if(R.failed)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

} // mrdox
} // clang

int
main(int argc, const char** argv)
{
    return clang::mrdox::testMain(argc, argv);
}
