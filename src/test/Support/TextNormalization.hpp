//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TEST_SUPPORT_TEXTNORMALIZATION_HPP
#define MRDOCS_TEST_SUPPORT_TEXTNORMALIZATION_HPP

#include <llvm/ADT/StringRef.h>
#include <string>
#include <string_view>

namespace mrdocs::test_support {

/** File format classification used by test normalizers. */
enum class OutputFormat
{
    html,
    adoc,
    xml,
    other,
};

/** Deduce the output format from a path or extension. */
OutputFormat
guessOutputFormat(llvm::StringRef pathOrExtension);

/** Normalize text for comparison in tests based on the output format. */
std::string
normalizeForComparison(std::string_view text, OutputFormat format);

/** Convenience overload that accepts a path or extension directly. */
std::string
normalizeForComparison(std::string_view text, llvm::StringRef pathOrExtension);

} // namespace mrdocs::test_support

#endif
