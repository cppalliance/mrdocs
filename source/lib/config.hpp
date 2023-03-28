// Copyright (c) 2023 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef MRDOX_CONFIG_HPP
#define MRDOX_CONFIG_HPP

#include <string>
#include <vector>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringSet.h>
#include <llvm/ADT/Optional.h>
#include <llvm/Support/ErrorOr.h>

namespace mrdox
{

struct config
{
  std::vector<std::string> skip_directories, ignore_namespaces, macro,
                            compile_database_commands;
  std::string compile_database;

};

llvm::ErrorOr<config> load_config(llvm::StringRef path);

}

#endif //MRDOX_CONFIG_HPP
