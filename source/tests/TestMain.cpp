//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "TestFiles.hpp"
#include "TestAction.hpp"
#include <llvm/Support/Signals.h>

// VFALCO GARBAGE
extern void force_xml_generator_linkage();

// Each test comes as a pair of files.
// A .cpp file containing valid declarations,
// and a .xml file containing the expected output
// of the XML generator, which must match exactly.

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// Generally Helpful Utilties
//
//------------------------------------------------

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
llvm::Expected<std::unique_ptr<tooling::ToolExecutor>>
createExecutor(
    tooling::CompilationDatabase const& files,
    std::vector<std::string> const& args,
    llvm::cl::OptionCategory& category,
    char const* overview)
{
    std::vector<const char*> argv;
    argv.reserve(args.size());
    for(auto const& arg : args)
        argv.push_back(arg.data());
#if 0
    int argc = static_cast<int>(argv.size());
    auto OptionsParser = CommonOptionsParser::create(
        argc, argv.data(), category, llvm::cl::ZeroOrMore, overview);
    if (!OptionsParser)
        return OptionsParser.takeError();
#endif
    auto executor = std::make_unique<
        tooling::StandaloneToolExecutor>(
            files,
            files.getAllFiles());
    if (!executor)
        return llvm::make_error<llvm::StringError>(
            "could not create StandaloneToolExecutor",
            llvm::inconvertibleErrorCode());
    return executor;
}

//------------------------------------------------

static
const char* toolOverview =
R"(Run tests from input files and report the results.

Example:
    $ mrdox_tests *( DIR )
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
    TestFiles files;
    for(int i = 1; i < argc; ++i)
        files.addDirectory(argv[i], R);
    auto args = makeVectorOfArgs(argc, argv);
    args.push_back("--executor=standalone");
    std::unique_ptr<tooling::ToolExecutor> executor;
    if(llvm::Error err = createExecutor(
        files, args, toolCategory, toolOverview).moveInto(executor))
    {
        return EXIT_FAILURE;
    }
    std::vector<std::string> Args;
    for(auto i = 0; i < argc; ++i)
        Args.push_back(argv[i]);
    Config cfg;
    llvm::Error err = executor->execute(std::make_unique<TestFactory>(cfg, R));
    R.success("execute", err);
    if(R.failed())
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
