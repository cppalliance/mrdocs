//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Generator.hpp>
#include <lib/Support/GeneratorRegistryImpl.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Dom/Value.hpp>
#include <mrdocs/Support/Error.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fstream>
#include <functional>


namespace mrdocs {

Generator::
~Generator() noexcept = default;

std::string
getGeneratorOutputPath(
    Generator const& generator,
    Corpus const& corpus)
{
    // A per-generator `output` (under generator-options.<id>) is used as
    // given. Otherwise the top-level `output` is used directly for a single
    // generator, or a subdirectory named after the generator id when several
    // run, so each writes into its own directory. All paths are resolved
    // relative to the configuration file.
    auto const& genOpts = corpus.config->generatorOptions;
    auto const it = genOpts.find(std::string(generator.id()));
    dom::Value const explicitOutput =
        it != genOpts.end() ? it->second.get("output") : dom::Value();
    if (explicitOutput.isString())
    {
        return files::normalizePath(files::makeAbsolute(
            std::string_view(explicitOutput.getString()),
            corpus.config->configDir()));
    }
    std::string absOutput = files::normalizePath(files::makeAbsolute(
        corpus.config->output, corpus.config->configDir()));
    if (corpus.config->generator.values.size() > 1)
    {
        absOutput = files::appendPath(absOutput, generator.id());
    }
    return absOutput;
}

Expected<void>
writeToFile(
    std::string_view fileName,
    std::function<Expected<void>(std::ostream&)> render)
{
    std::string dir(files::getParentDir(fileName));
    MRDOCS_TRY(files::createDirectory(dir));

    std::ofstream os;
    try
    {
        os.open(std::string(fileName),
            std::ios_base::binary |
                std::ios_base::out |
                std::ios_base::trunc);
    }
    catch(std::exception const& ex)
    {
        return Unexpected(formatError("std::ofstream threw \"{}\"", ex.what()));
    }

    try
    {
        return render(os);
    }
    catch(std::exception const& ex)
    {
        return Unexpected(formatError("writing \"{}\" threw \"{}\"",
            fileName, ex.what()));
    }
}

Expected<std::string>
getSinglePageFullPath(
    std::string_view outputPath,
    std::string_view extension)
{
    namespace path = llvm::sys::path;
    using SmallString = llvm::SmallString<0>;

    // When the path looks like a file, it is the single page itself.
    if (files::looksLikeFile(outputPath))
    {
        return std::string(outputPath);
    }

    // Otherwise the path is a directory and the single page is created
    // inside it as reference.<ext>.
    SmallString ext(".");
    ext += extension;
    SmallString fileName(outputPath);
    path::append(fileName, "reference");
    path::replace_extension(fileName, ext);
    return fileName.str().str();
}

Expected<void>
installGenerator(std::unique_ptr<Generator> G)
{
    return getGeneratorRegistryImpl().insert(std::move(G));
}

Generator const*
findGenerator(std::string_view id) noexcept
{
    return getGeneratorRegistryImpl().find(id);
}

} // mrdocs

