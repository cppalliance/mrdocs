//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "BitcodeReader.h"
#include "BitcodeWriter.h"
#include "Generators.h"
#include "ClangDoc.h"
#include <mrdox/ClangDocContext.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/ThreadPool.h>

namespace clang {
namespace mrdox {

namespace {

const char* Overview =
R"(Generates documentation from source code and comments.

Example usage for files without flags (default):

  $ mrdox File1.cpp File2.cpp ... FileN.cpp

Example usage for a project using a compile commands database:

  $ mrdox --executor=all-TUs compile_commands.json
)";

static
llvm::cl::extrahelp
CommonHelp(
    tooling::CommonOptionsParser::HelpMessage);

static
llvm::cl::OptionCategory
MrDoxCategory("mrdox options");

static
llvm::cl::opt<std::string>
ProjectName(
    "project-name",
    llvm::cl::desc("Name of project."),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<bool>
    IgnoreMappingFailures(
        "ignore-map-errors",
        llvm::cl::desc("Continue if files are not mapped correctly."),
        llvm::cl::init(true),
        llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<std::string>
OutDirectory(
    "output",
    llvm::cl::desc("Directory for outputting generated files."),
    llvm::cl::init("docs"),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<bool>
PublicOnly(
    "public",
    llvm::cl::desc("Document only public declarations."),
    llvm::cl::init(false),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<bool>
DoxygenOnly(
    "doxygen",
    llvm::cl::desc("Use only doxygen-style comments to generate docs."),
    llvm::cl::init(false),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::list<std::string>
UserStylesheets(
    "stylesheets",
    llvm::cl::CommaSeparated,
    llvm::cl::desc("CSS stylesheets to extend the default styles."),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<std::string>
SourceRoot(
    "source-root", llvm::cl::desc(R"(
Directory where processed files are stored.
Links to definition locations will only be
generated if the file is in this dir.)"),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<std::string>
RepositoryUrl(
    "repository", llvm::cl::desc(R"(
URL of repository that hosts code.
Used for links to definition locations.)"),
    llvm::cl::cat(MrDoxCategory));

enum OutputFormatTy
{
    adoc,
    xml
};

static
llvm::cl::opt<OutputFormatTy>
FormatEnum(
    "format",
    llvm::cl::desc("Format for outputted docs."),
    llvm::cl::values(
        clEnumValN(OutputFormatTy::adoc, "adoc",
            "Documentation in Asciidoc format."),
        clEnumValN(OutputFormatTy::xml, "xml",
            "Documentation in XML format.")),
    llvm::cl::init(OutputFormatTy::adoc), // default value
    llvm::cl::cat(MrDoxCategory));

std::string
getFormatString()
{
    switch (FormatEnum)
    {
    case OutputFormatTy::adoc:
        return "adoc";
    case OutputFormatTy::xml:
        return "xml";
    }
    llvm_unreachable("Unknown OutputFormatTy");
}

} // (anon)

//------------------------------------------------

ClangDocContext::
ClangDocContext()
{
    llvm::SmallString<128> SourceRootDir;
    llvm::sys::fs::current_path(SourceRootDir);
    SourceRoot = std::string(SourceRootDir.str());
}

llvm::Error
setupContext(
    ClangDocContext& CDCtx,
    int argc, const char** argv)
{
    llvm::SmallVector<llvm::StringRef, 16> args;
    for(int i = 0; i < argc; ++i)
        args.push_back(argv[i]);
    return setupContext(CDCtx, args);
}

llvm::Error
setupContext(
    ClangDocContext& CDCtx,
    llvm::SmallVector<llvm::StringRef, 16> const& args)
{
    llvm::SmallVector<char const*> argv;
    for(auto const& s : args)
        argv.push_back(s.data());
    int argc = argv.size();
    if(llvm::Error err = clang::tooling::createExecutorFromCommandLineArgs(
        argc,
        argv.begin(),
        MrDoxCategory,
        Overview).moveInto(CDCtx.Executor))
    {
        return err;
    }

    // Fail early if an invalid format was provided.
    std::string Format = getFormatString();
    llvm::outs() << "Emiting docs in " << Format << " format.\n";
    if(llvm::Error err =
        mrdox::findGeneratorByName(Format).moveInto(CDCtx.G))
        return err;

    if (! DoxygenOnly)
        CDCtx.ArgAdjuster = tooling::combineAdjusters(
            getInsertArgumentAdjuster(
                "-fparse-all-comments",
                tooling::ArgumentInsertPosition::END),
            CDCtx.ArgAdjuster);

    CDCtx.ECtx = CDCtx.Executor->getExecutionContext(),
    CDCtx.ProjectName = ProjectName;
    CDCtx.PublicOnly = PublicOnly;
    CDCtx.OutDirectory = OutDirectory;
    CDCtx.SourceRoot = SourceRoot;
    CDCtx.RepositoryUrl = RepositoryUrl;
    CDCtx.IgnoreMappingFailures = IgnoreMappingFailures;
    CDCtx.OutDirectory = OutDirectory;
    return llvm::Error::success();
}

//------------------------------------------------

llvm::Error
buildIndex(
    ClangDocContext& CDCtx)
{
    //
    // Mapping phase
    //
    {
        llvm::outs() << "Mapping declarations\n";
        auto Err = CDCtx.Executor->execute(
            newMapperActionFactory(CDCtx),
            CDCtx.ArgAdjuster);
        if(Err)
        {
            if(! CDCtx.IgnoreMappingFailures)
            {
                llvm::errs() <<
                    "Mapping failure: " << Err << "\n";
                return Err;
            }

            llvm::errs() <<
                "GotFailure mapping decls in files. mrdox will ignore "
                "these files and continue:\n" <<
                toString(std::move(Err)) << "\n";
        }
    }

    // Collect values into output by key.
    // In ToolResults, the Key is the hashed USR and the value is the
    // bitcode-encoded representation of the Info object.
    llvm::outs() << "Collecting symbols\n";
    llvm::StringMap<std::vector<StringRef>> USRToBitcode;
    CDCtx.Executor->getToolResults()->forEachResult(
        [&](StringRef Key, StringRef Value)
        {
            auto R = USRToBitcode.try_emplace(Key, std::vector<StringRef>());
            R.first->second.emplace_back(Value);
        });

    // Collects all Infos according to their unique USR value. This map is added
    // to from the thread pool below and is protected by the USRToInfoMutex.
    llvm::sys::Mutex USRToInfoMutex;

    // First reducing phase (reduce all decls into one info per decl).
    llvm::outs() << "Reducing " << USRToBitcode.size() << " declarations\n";
    std::atomic<bool> GotFailure;
    GotFailure = false;
    llvm::sys::Mutex IndexMutex;
    // ExecutorConcurrency is a flag exposed by AllTUsExecution.h
    llvm::ThreadPool Pool(llvm::hardware_concurrency(tooling::ExecutorConcurrency));
    for (auto& Group : USRToBitcode)
    {
        Pool.async([&]()
        {
            std::vector<std::unique_ptr<mrdox::Info>> Infos;

            for (auto& Bitcode : Group.getValue())
            {
                llvm::BitstreamCursor Stream(Bitcode);
                mrdox::ClangDocBitcodeReader Reader(Stream);
                auto ReadInfos = Reader.readBitcode();
                if (!ReadInfos)
                {
                    llvm::errs() << toString(ReadInfos.takeError()) << "\n";
                    GotFailure = true;
                    return;
                }
                std::move(
                    ReadInfos->begin(),
                    ReadInfos->end(),
                    std::back_inserter(Infos));
            }

            auto Reduced = mrdox::mergeInfos(Infos);
            if (!Reduced)
            {
                // VFALCO What about GotFailure?
                llvm::errs() << llvm::toString(Reduced.takeError());
                return;
            }

            // Add a reference to this Info in the Index
            {
                std::lock_guard<llvm::sys::Mutex> Guard(IndexMutex);
                clang::mrdox::Generator::addInfoToIndex(CDCtx.Idx, Reduced.get().get());
            }

            // Save in the result map (needs a lock due to threaded access).
            {
                std::lock_guard<llvm::sys::Mutex> Guard(USRToInfoMutex);
                CDCtx.USRToInfo[Group.getKey()] = std::move(Reduced.get());
            }
        });
    }

    Pool.wait();

    if (GotFailure)
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "an error occurred");

    return llvm::Error::success();
}

} // mrdox
} // clang
