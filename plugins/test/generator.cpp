//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Plugin.hpp>

struct myGen final : clang::mrdox::Generator
{
  std::string_view
  id() const noexcept override
  {
    return "test";
  }

  std::string_view
  displayName() const noexcept override
  {
    return "test generator";
  }

  std::string_view
  fileExtension() const noexcept override
  {
    return "test";
  }

  clang::mrdox::Error
  buildOne(
      std::ostream& os,
      clang::mrdox::Corpus const& corpus) const override
  {
    os << "It works!";
    return {};
  }
};

std::unique_ptr<clang::mrdox::Generator> clang::mrdox::makeMrDoxGenerator()
{
  return std::make_unique<myGen>();
}
