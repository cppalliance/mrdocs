// Copyright (c) 2023 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <llvm/Support/raw_ostream.h>
#include "config.hpp"
#include "llvm/Support/Errc.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"

LLVM_YAML_IS_SEQUENCE_VECTOR(llvm::StringSet<>);

template <>
struct llvm::yaml::MappingTraits<mrdox::config> {
  static void mapping(IO &io, mrdox::config &info)
  {
    io.mapOptional("skip-directories",          info.skip_directories);
    io.mapOptional("ignore-namespaces",         info.ignore_namespaces);
    io.mapOptional("macro",                     info.macro);
    io.mapOptional("compile-database-commands", info.compile_database_commands);
    io.mapOptional("compile-database",          info.compile_database);
  }
};

namespace mrdox
{

llvm::ErrorOr<config> load_config(llvm::StringRef path)
{
  auto ct = llvm::MemoryBuffer::getFile(path);
  if (!ct)
    return ct.getError();

  llvm::yaml::Input yin{**ct};
  config res;
  yin >> res;
  if ( yin.error() )
    return yin.error();

  return res;
}

}