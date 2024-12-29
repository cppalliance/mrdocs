// Check if all std includes are functional with -nostdinc
// defined in MrDocsCompilationDatabase.cpp and headers/libc-stubs

// libcxx
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <barrier>
#include <bit>
#include <bitset>
#include <cassert>
#include <ccomplex>
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <charconv>
#include <chrono>
#include <cinttypes>
#include <ciso646>
#include <climits>
#include <clocale>
#include <cmath>
#include <codecvt>
#include <compare>
#include <complex>
#include <complex.h>
#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <csetjmp>
#include <csignal>
#include <cstdalign>
#include <cstdarg>
#include <cstdbool>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctgmath>
#include <ctime>
#include <ctype.h>
#include <cuchar>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <errno.h>
#include <exception>
#include <execution>
#include <expected>
#include <experimental/iterator>
#include <experimental/memory>
#include <experimental/propagate_const>
#include <experimental/simd>
#include <experimental/type_traits>
#include <experimental/utility>
#include <ext/hash_map>
#include <ext/hash_set>
#include <fenv.h>
#include <filesystem>
#include <flat_map>
#include <float.h>
#include <format>
#include <forward_list>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <inttypes.h>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <latch>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <math.h>
#include <mdspan>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <numbers>
#include <numeric>
#include <optional>
#include <ostream>
#include <print>
#include <queue>
#include <random>
#include <ranges>
#include <ratio>
#include <regex>
#include <scoped_allocator>
#include <semaphore>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <sstream>
#include <stack>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <stop_token>
#include <streambuf>
#include <string>
#include <string.h>
#include <string_view>
#include <strstream>
#include <syncstream>
#include <system_error>
#include <tgmath.h>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <uchar.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>
#include <version>
#include <wchar.h>
#include <wctype.h>

/** Computes the square root of an integral value.

    This function calculates the square root of a
    given integral value using bit manipulation.

    @throws std::invalid_argument if the input value is negative.

    @tparam T The type of the input value. Must be an integral type.
    @param value The integral value to compute the square root of.
    @return The square root of the input value.
 */
template <typename T>
std::enable_if_t<std::is_integral_v<T>, T>
sqrt(T value);

