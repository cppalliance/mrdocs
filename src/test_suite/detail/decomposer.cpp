//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#include "decomposer.hpp"

#if __has_include(<cxxabi.h>)
#    include <cxxabi.h>
#endif

namespace test_suite {
namespace detail {
std::string demangle(const char *mangled)
{
#if __has_include(<cxxabi.h>)
    int status;
    char *demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    std::string result;
    if (status == 0) {
        result = demangled;
    } else {
        result = mangled;
    }
    std::free(demangled);
    return result;
#else
    return mangled;
#endif
}
} // detail
} // test_suite
