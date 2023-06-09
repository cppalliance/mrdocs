//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "GeneratorsImpl.hpp"
#include "Path.hpp"
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/FileSystem.h>



namespace clang {
namespace mrdox {

extern
std::unique_ptr<Generator>
makeAdocGenerator();

extern
std::unique_ptr<Generator>
makeBitcodeGenerator();

extern
std::unique_ptr<Generator>
makeXMLGenerator();

Generators::
~Generators() noexcept = default;

void
GeneratorsImpl::
refresh_plist()
{
    plist_.clear();
    plist_.reserve(list_.size());
    for(auto const& g : list_)
        plist_.push_back(g.get());
}

extern llvm::cl::opt<std::string>   PluginsPath;

GeneratorsImpl::
GeneratorsImpl()
{
    Error err;
    err = insert(makeAdocGenerator());
    err = insert(makeBitcodeGenerator());
    err = insert(makeXMLGenerator());

    std::string err_;

    SmallPathString plugin_path;
    if (!PluginsPath.empty())
      plugin_path = PluginsPath;
    else
      llvm::sys::fs::current_path(plugin_path);

    std::error_code ec;
    for (auto itr = llvm::sys::fs::directory_iterator(plugin_path, ec);
         itr != llvm::sys::fs::directory_iterator() && !ec;
         itr.increment(ec))
    {
      if (!itr->path().ends_with(".dll")
       && !itr->path().ends_with(".so"))
        continue;

      auto lib = llvm::sys::DynamicLibrary::getPermanentLibrary(itr->path().c_str(), &err_);
      auto mkgen = lib.getAddressOfSymbol("makeMrDoxGenerator");

      if (mkgen)
        err = insert(reinterpret_cast<decltype(&makeAdocGenerator)>(mkgen)());
    }

    if (ec)
      throw std::system_error(ec);
}

Generator const*
GeneratorsImpl::
find(
    std::string_view id) const noexcept
{
    for(std::size_t i = 0; i < list_.size(); ++i)
        if(list_[i]->id() == id)
            return list_[i].get();
    return nullptr;
}

Error
GeneratorsImpl::
insert(
    std::unique_ptr<Generator> G)
{
    if(find(G->id()) != nullptr)
        return Error("generator id=\"{}\" already exists", G->id());
    list_.emplace_back(std::move(G));
    refresh_plist();
    return Error::success();
}


//------------------------------------------------

GeneratorsImpl&
getGeneratorsImpl() noexcept
{
    static GeneratorsImpl impl;
    return impl;
}

Generators const&
getGenerators() noexcept
{
    return getGeneratorsImpl();
}

} // mrdox
} // clang
