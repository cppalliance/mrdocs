//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TOOL_TOOLARGS_HPP
#define MRDOCS_TOOL_TOOLARGS_HPP

#include <llvm/Support/CommandLine.h>
#include <mrdocs/Config.hpp>
#include <string>

namespace clang {
namespace mrdocs {

/** Command line options and tool settings.
*/
class ToolArgs
{
    ToolArgs();

    llvm::cl::OptionCategory    cmdLineCat;
    llvm::cl::OptionCategory    pathsCat;
    llvm::cl::OptionCategory    buildOptsCat;
    llvm::cl::OptionCategory    generatorCat;
    llvm::cl::OptionCategory    filtersCat;
    llvm::cl::OptionCategory    extraCat;


public:
    static ToolArgs instance_;

    char const*                 usageText;
    llvm::cl::extrahelp         extraHelp;

    // ------------------------------------------------
    // Command line options
    llvm::cl::list<std::string> inputs;
    llvm::cl::opt<std::string> config;
    llvm::cl::opt<int> concurrency;

    // ------------------------------------------------
    // Common options
    /// Path to the source root directory
    llvm::cl::opt<std::string> sourceRoot;

    /// Directory or file for generating output
    llvm::cl::opt<std::string> output;

    /// Path to the compilation database or build script
    llvm::cl::opt<std::string> compilationDatabase;

    /// Arguments to pass to CMake when generating the compilation database
    /// from CMakeLists.txt
    llvm::cl::opt<std::string> cmake;

    // /// Paths to the LibC++ standard library
    // llvm::cl::list<std::string> libCxxPaths;

    /// Additional defines passed to the compiler
    llvm::cl::list<std::string> defines;

    /// Documentation generator. Supported generators are: adoc, html, xml
    llvm::cl::opt<std::string> generate;

    /// Path to the Addons directory
    llvm::cl::opt<std::string> addons;

    /// True if output should consist of multiple files)
    llvm::cl::opt<bool> multipage;

    /// Base URL for links to source code
    llvm::cl::opt<std::string> baseURL;

    /// True if more information should be output to the console)
    llvm::cl::opt<bool> verbose;

    /// The minimum reporting level: 0 to 4
    llvm::cl::opt<unsigned> report;

    /// Continue if files are not mapped correctly)
    llvm::cl::opt<bool> ignoreMapErrors;

    /// Whether AST visitation failures should not stop the program)
    llvm::cl::opt<bool> ignoreFailures;

    /// Extraction policy for references to external declarations.
    llvm::cl::opt<std::string> referencedDeclarations;

    /// Extraction policy for anonymous namespace.
    llvm::cl::opt<std::string> anonymousNamespaces;

    /// Extraction policy for inaccessible members.
    llvm::cl::opt<std::string> inaccessibleMembers;

    /// Extraction policy for inaccessible bases.
    llvm::cl::opt<std::string> inaccessibleBases;

    /// Specifies files that should be filtered
    struct FileFilter
    {
        /// Constructor
        FileFilter(llvm::cl::OptionCategory& cat);

        /// Directories to include
        llvm::cl::list<std::string> include;

        /// File patterns
        llvm::cl::list<std::string> filePatterns;
    };

    /// @copydoc FileFilter
    FileFilter input;

    /// Specifies filters for various kinds of symbols.
    struct Filters
    {
        /// Constructor
        Filters(llvm::cl::OptionCategory& cat);

        /// Specifies inclusion and exclusion patterns
        struct Category
        {
            Category(llvm::cl::OptionCategory& cat);

            llvm::cl::list<std::string> include;
            llvm::cl::list<std::string> exclude;
        };

        /// Specifies filter patterns for symbols
        Category symbols;
    };

    /// @copydoc Filters
    Filters filters;

    /** Namespace for symbols rendered as "see-below".
     */
    llvm::cl::list<std::string> seeBelow;

    /** Namespace for symbols rendered as "implementation-defined".
     */
    llvm::cl::list<std::string> implementationDefined;

    /** Whether to detect the SFINAE idiom.
     */
    llvm::cl::opt<bool> detectSfinae;

    /// Apply the command line options to the settings
    Expected<void>
    apply(Config::Settings& c, std::string_view execPath, char const** argv);

    // Hide all options that don't belong to us
    void
    hideForeignOptions();
};

/** Command line arguments passed to the tool.

    This is a global variable because of how the
    LLVM command line interface is designed.
*/
constexpr static ToolArgs& toolArgs = ToolArgs::instance_;

// This static assertion checks that required options are defined
// in the ToolArgs class. If this assertion fails, it means that
// the ToolArgs class is missing some required options.
static_assert(detail::DefinesAllOptions<ToolArgs>);

} // mrdocs
} // clang

#endif
