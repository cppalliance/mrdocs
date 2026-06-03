//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_SCRIPT_SCRIPTGENERATOR_HPP
#define MRDOCS_LIB_GEN_SCRIPT_SCRIPTGENERATOR_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <iosfwd>
#include <string>
#include <string_view>

namespace mrdocs::script {

/** A generator whose output is produced by a user script.

    A script-driven generator hands the whole emit to a Lua or
    JavaScript entry point of the form
    `generate(corpus, output, config, params)`: the script traverses the
    corpus and writes whatever files it wants through the `output`
    object, optionally reading the resolved `config` and its own
    `params`. Because the script owns the output structure, it can
    produce shapes the per-page generators cannot, such as a single
    artifact aggregated across all symbols (a search index, for example).
*/
class ScriptGenerator
    : public Generator
{
    std::string id_;
    // The absolute path to the Lua or JavaScript entry script.
    std::string scriptPath_;
    // The generator's own parameters, from the manifest's `params`
    // mapping; passed to the entry script as its `params` argument.
    dom::Object params_;

public:
    /** Construct a script-driven generator.

        @param id The generator id, used to select it on the command
            line.
        @param scriptPath The absolute path to the entry script.
        @param params The generator's own parameters, from its manifest.
    */
    ScriptGenerator(
        std::string id,
        std::string scriptPath,
        dom::Object params);

    std::string_view
    id() const noexcept override;

    std::string_view
    displayName() const noexcept override;

    std::string_view
    fileExtension() const noexcept override;

    /** Run the entry script, which owns the whole emit.
    */
    Expected<void>
    build(
        std::string_view outputPath,
        Corpus const& corpus) const override;

    /** Reject single-page output.

        A script-driven generator owns its output structure and writes
        whatever files it wants, so there is no single-stream form.
    */
    Expected<void>
    buildOne(
        std::ostream& os,
        Corpus const& corpus) const override;
};

/** Discover script-driven generators and install them.

    For each configured addon root, walk the immediate subdirectories of
    <root>/generator/. A subdirectory becomes a script-driven generator
    when its `mrdocs-generator.yml` names an entry script. The generator
    id, used to select it on the command line, is the subdirectory name.

    Should be called once after the configuration is resolved and before
    a generator is looked up by id.
*/
Expected<void>
discoverScriptGenerators(Config::Settings const& settings);

} // mrdocs::script

#endif
