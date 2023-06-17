//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef MRDOX_TEST_MACROS_H
#define MRDOX_TEST_MACROS_H

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <type_traits>
#include <string_view>
#include <sstream>

#if __has_include(<cxxabi.h>)

#include <cxxabi.h>

#endif

// These are test macros we can use to test our code without having to
// integrate a test framework for now.

namespace test::detail {
    template<typename T, typename = void>
    struct has_ostream_op : std::false_type {
    };

    template<typename T>
    struct has_ostream_op<T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<const T &>())>>
            : std::true_type {
    };

    std::string demangle(const char *mangled) {
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

    template<class T>
    std::string format_value(const T &value) {
        std::string out;
        if constexpr (fmt::has_formatter<T, fmt::format_context>::value) {
            out += fmt::format("{}", value);
        } else if constexpr (has_ostream_op<T>::value) {
            std::stringstream ss;
            ss << value;
            out += ss.str();
        } else {
            out += fmt::format("{}", demangle(typeid(T).name()));
        }
        return out;
    }

    template<class T, class U>
    class binary_operands {
        bool result_;
        T lhs_;
        std::string_view op_;
        U rhs_;
    public:
        binary_operands(bool comparisonResult, T lhs, std::string_view op, U rhs)
                : result_(comparisonResult),
                  lhs_(lhs),
                  op_(op),
                  rhs_(rhs) {}

        [[nodiscard]] bool get_result() const {
            return result_;
        }

        [[nodiscard]] std::string format() const {
            return fmt::format("{} {} {}", format_value(lhs_), op_, format_value(rhs_));
        }
    };

    // Wraps the first element in an expression so that other elements are also evaluated as wrappers
    // when compared with it
    template<class T>
    class first_operand {
        T lhs_;

    public:
        explicit first_operand(T lhs) : lhs_(lhs) {}

        template<class U>
        friend
        binary_operands<T, U const &> operator==(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ == rhs ), lhs.lhs_, "==", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &> operator!=(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ != rhs ), lhs.lhs_, "!=", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &> operator<(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ < rhs ), lhs.lhs_, "<", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &> operator<=(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ <= rhs ), lhs.lhs_, "<=", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &> operator>(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ > rhs ), lhs.lhs_, ">", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &> operator>=(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ >= rhs ), lhs.lhs_, ">=", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &> operator|(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ | rhs ), lhs.lhs_, "|", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &> operator&(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ & rhs ), lhs.lhs_, "&", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &> operator^(first_operand &&lhs, U &&rhs) {
            return {static_cast<bool>( lhs.lhs_ ^ rhs ), lhs.lhs_, "^", rhs};
        }

        // && and || are not supported because of operator precedence
        // use two requires instead of &&
        template<class U>
        friend
        binary_operands<T, U const &> operator&&(first_operand &&lhs, U &&rhs) = delete;

        // && and || are not supported because of operator precedence
        // use parenthesis instead of ||
        template<class U>
        friend
        binary_operands<T, U const &> operator||(first_operand &&lhs, U &&rhs) = delete;

        [[nodiscard]] std::string format() const {
            return format_value(lhs_);
        }
    };

    // Converts the first elements in the expression to an first_operand wrapper
    // first_operand wrapper will then wrap the other elements in the expression
    // These wrappers allow the application to have access to all elements in an
    // expression and evaluate them as needed to generate proper error messages
    struct decomposer {
        template<class T>
        friend auto operator<=(decomposer &&, T &&lhs) -> first_operand<T const &> {
            return first_operand<T const &>{lhs};
        }
    };
}

#define DETAIL_STRINGIFY(...) #__VA_ARGS__

#if defined(__clang__)
#    define DETAIL_START_WARNINGS_SUPPRESSION _Pragma( "clang diagnostic push" )
#    define DETAIL_STOP_WARNINGS_SUPPRESSION  _Pragma( "clang diagnostic pop" )
#    define DETAIL_SUPPRESS_PARENTHESES_WARNINGS \
         _Pragma( "clang diagnostic ignored \"-Wparentheses\"" )
#elif defined(__GNUC__) && !defined(__clang__) && !defined(__ICC) && !defined(__CUDACC__) && !defined(__LCC__) && !defined(__NVCOMPILER)
#    define DETAIL_START_WARNINGS_SUPPRESSION _Pragma( "GCC diagnostic push" )
#    define DETAIL_STOP_WARNINGS_SUPPRESSION  _Pragma( "GCC diagnostic pop" )
#    define DETAIL_SUPPRESS_PARENTHESES_WARNINGS \
         _Pragma( "GCC diagnostic ignored \"-Wparentheses\"" )
#else
#    define DETAIL_START_WARNINGS_SUPPRESSION
#    define DETAIL_STOP_WARNINGS_SUPPRESSION
#    define DETAIL_SUPPRESS_PARENTHESES_WARNINGS
#endif

#define DETAIL_REQUIRE(macro_name, operation, ...) \
  if (!(operation static_cast<bool>(__VA_ARGS__))) { \
    DETAIL_START_WARNINGS_SUPPRESSION \
    DETAIL_SUPPRESS_PARENTHESES_WARNINGS \
    fmt::println("{} failed:\n    {} ({})\n    file: {}\n    line: {}", DETAIL_STRINGIFY(macro_name), DETAIL_STRINGIFY(__VA_ARGS__), (test::detail::decomposer() <= __VA_ARGS__).format(), __FILE__, __LINE__); \
    DETAIL_STOP_WARNINGS_SUPPRESSION  \
    return EXIT_FAILURE; \
  } \
  (void) 0

#define REQUIRE(...) DETAIL_REQUIRE(REQUIRE, true ==, __VA_ARGS__)

#define REQUIRE_FALSE(...) DETAIL_REQUIRE(REQUIRE_FALSE, false ==, __VA_ARGS__)

#endif //MRDOX_TEST_MACROS_H
