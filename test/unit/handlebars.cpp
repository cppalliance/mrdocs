//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include <mrdox/Support/Handlebars.hpp>
#include <mrdox/Support/Path.hpp>
#include "test_macros.hpp"

int
main() {
    using namespace clang::mrdox;
    auto fileTextExp = files::getFileText(MRDOX_UNIT_TEST_DIR "/fixtures/function.adoc.hbs");
    REQUIRE(fileTextExp);
    auto template_str = *fileTextExp;
    REQUIRE_FALSE(template_str.empty());
    return EXIT_SUCCESS;
}
