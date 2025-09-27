//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "MrDocsSettingsDB.hpp"
#include <mrdocs/Support/Path.hpp>


namespace mrdocs {

MrDocsSettingsDB::MrDocsSettingsDB(ConfigImpl const& config)
{
    std::vector<std::string> sourceFiles;
    auto& s = config.settings();
    for (auto const& curInput: s.input)
    {
        forEachFile(
            curInput,
            s.recursive,
            [&](std::string_view path) -> Expected<void> {
            if (files::isDirectory(path))
            {
                return {};
            }
            // Check file patterns that should match
            auto inputFilename = files::getFileName(path);
            if (std::ranges::none_of(
                    s.filePatterns,
                    [&](PathGlobPattern const& pattern) {
                return pattern.match(inputFilename);
            }))
            {
                return {};
            }
            // Check file or directories in path that should be skipped
            if (std::ranges::any_of(s.exclude, [&](std::string const& excludePath) {
                return files::startsWith(path, excludePath);
            }))
            {
                return {};
            }
            // Check if the relative path matches any of the s.excludePatterns
            if (std::ranges::any_of(
                    s.excludePatterns,
                    [&](PathGlobPattern const& pattern) {
                return pattern.match(path);
            }))
            {
                return {};
            }
            sourceFiles.emplace_back(path);
            return {};
        });
    }

    for (auto const& pathName: sourceFiles)
    {
        // auto fileName = files::getFileName(pathName);
        auto parentDir = files::getParentDir(pathName);

        std::vector<std::string> cmds;
        cmds.emplace_back("clang");
        cmds.emplace_back("-fsyntax-only");
        cmds.emplace_back("-std=c++23");
        cmds.emplace_back("-pedantic-errors");
        cmds.emplace_back("-Werror");
        cmds.emplace_back("-x");
        cmds.emplace_back("c++");
        cmds.emplace_back(pathName);
        cc_.emplace_back(parentDir, pathName, std::move(cmds), parentDir);
        cc_.back().Heuristic = "generated from mrdocs.yml";
    }
}

std::vector<clang::tooling::CompileCommand>
MrDocsSettingsDB::getCompileCommands(llvm::StringRef FilePath) const
{
    std::vector<clang::tooling::CompileCommand> result;
    for (auto const& cmd: cc_)
    {
        if (cmd.Filename == FilePath)
        {
            result.push_back(cmd);
        }
    }
    return result;
}

std::vector<std::string>
MrDocsSettingsDB::getAllFiles() const
{
    std::vector<std::string> result;
    result.reserve(cc_.size());
    for (auto const& cmd: cc_)
    {
        result.push_back(cmd.Filename);
    }
    return result;
}

std::vector<clang::tooling::CompileCommand>
MrDocsSettingsDB::getAllCompileCommands() const
{
    return cc_;
}

} // namespace mrdocs
