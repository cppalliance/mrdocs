//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef MRDOCS_TEST_DECOMPOSER_HPP
#define MRDOCS_TEST_DECOMPOSER_HPP

#include <type_traits>
#include <string_view>
#include <sstream>
#include <utility>

#if defined(__has_include) && __has_include(<fmt/format.h>)
#include <fmt/format.h>
#include <fmt/ostream.h>
#define MRDOCS_TEST_HAS_FMT
#endif

// These are test macros we can use to test our code without having to
// integrate a test framework for now.

namespace test_suite::detail
{
    template<typename T, typename = void>
    struct has_ostream_op : std::false_type
    {};

    template<typename T>
    struct has_ostream_op<T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<const T &>())>>
            : std::true_type
    {};

    std::string
    demangle(const char *mangled);

    template <class T>
    std::string
    demangle()
    {
        return demangle(typeid(T).name());
    }

    template<class T>
    std::string
    format_value(T const& value)
    {
        std::string out;
        if constexpr (std::is_convertible_v<std::decay_t<T>, std::string_view>)
        {
            out += '\"';
        }
#ifdef MRDOCS_TEST_HAS_FMT
        if constexpr (fmt::has_formatter<T, fmt::format_context>::value) {
            out += fmt::format("{}", value);
        } else
#endif
        if constexpr (has_ostream_op<T>::value) {
            std::stringstream ss;
            ss << value;
            out += ss.str();
        } else {
            out += demangle<T>();
        }
        if constexpr (std::is_convertible_v<std::decay_t<T>, std::string_view>)
        {
            out += '\"';
        }
        return out;
    }

    template<class T, class U>
    class binary_operands
    {
        bool result_;
        T lhs_;
        std::string_view op_;
        U rhs_;

    public:
        binary_operands(
            bool result, T lhs, std::string_view op, U rhs)
                : result_(result), lhs_(lhs), op_(op), rhs_(rhs) {}

        [[nodiscard]]
        bool
        get_result() const
        {
            return result_;
        }

        [[nodiscard]]
        std::string
        format() const
        {
            std::string out;
            out += format_value(lhs_);
            out += " ";
            out += op_;
            out += " ";
            out += format_value(rhs_);
            return out;
        }
    };

    // Wraps the first element in an expression so that other
    // elements can also be evaluated as wrappers when compared
    // with it
    template<class T>
    class first_operand
    {
        T lhs_;

        template <class U>
        static constexpr bool integral_comparison =
            std::integral<std::decay_t<T>> && std::integral<std::decay_t<U>>;

    public:
        explicit first_operand(T lhs) : lhs_(lhs) {}

        template<class U>
        friend
        binary_operands<T, U const &>
        operator==(first_operand &&lhs, U &&rhs)
        {
            if constexpr (integral_comparison<U>)
            {
                return {std::cmp_equal( lhs.lhs_, rhs ), lhs.lhs_, "==", rhs};
            }
            else
            {
                return {static_cast<bool>( lhs.lhs_ == rhs ), lhs.lhs_, "==", rhs};
            }
        }

        template<class U>
        friend
        binary_operands<T, U const &>
        operator!=(first_operand &&lhs, U &&rhs)
        {
            if constexpr (integral_comparison<U>)
            {
                return {std::cmp_not_equal( lhs.lhs_, rhs ), lhs.lhs_, "!=", rhs};
            }
            else
            {
                return {static_cast<bool>( lhs.lhs_ != rhs ), lhs.lhs_, "!=", rhs};
            }
        }

        template<class U>
        friend
        binary_operands<T, U const &>
        operator<(first_operand &&lhs, U &&rhs)
        {
            if constexpr (integral_comparison<U>)
            {
                return {std::cmp_less( lhs.lhs_, rhs ), lhs.lhs_, "<", rhs};
            }
            else
            {
                return {static_cast<bool>( lhs.lhs_ < rhs ), lhs.lhs_, "<", rhs};
            }
        }

        template<class U>
        friend
        binary_operands<T, U const &>
        operator<=(first_operand &&lhs, U &&rhs)
        {
            if constexpr (integral_comparison<U>)
            {
                return {std::cmp_less_equal( lhs.lhs_, rhs ), lhs.lhs_, "<=", rhs};
            }
            else
            {
                return {static_cast<bool>( lhs.lhs_ <= rhs ), lhs.lhs_, "<=", rhs};
            }
        }

        template<class U>
        friend
        binary_operands<T, U const &>
        operator>(first_operand &&lhs, U &&rhs)
        {
            if constexpr (integral_comparison<U>)
            {
                return {std::cmp_greater( lhs.lhs_, rhs ), lhs.lhs_, ">", rhs};
            }
            else
            {
                return {static_cast<bool>( lhs.lhs_ > rhs ), lhs.lhs_, ">", rhs};
            }
        }

        template<class U>
        friend
        binary_operands<T, U const &>
        operator>=(first_operand &&lhs, U &&rhs)
        {
            if constexpr (integral_comparison<U>)
            {
                return {std::cmp_greater_equal( lhs.lhs_, rhs ), lhs.lhs_, ">=", rhs};
            }
            else
            {
                return {static_cast<bool>( lhs.lhs_ >= rhs ), lhs.lhs_, ">=", rhs};
            }
        }

        template<class U>
        friend
        binary_operands<T, U const &>
        operator|(first_operand &&lhs, U &&rhs)
        {
            return {static_cast<bool>( lhs.lhs_ | rhs ), lhs.lhs_, "|", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &>
        operator&(first_operand &&lhs, U &&rhs)
        {
            return {static_cast<bool>( lhs.lhs_ & rhs ), lhs.lhs_, "&", rhs};
        }

        template<class U>
        friend
        binary_operands<T, U const &>
        operator^(first_operand &&lhs, U &&rhs)
        {
            return {static_cast<bool>( lhs.lhs_ ^ rhs ), lhs.lhs_, "^", rhs};
        }

        // && and || are not supported because of operator precedence
        // use two requires instead of &&
        template<class U>
        friend
        binary_operands<T, U const &>
        operator&&(first_operand &&lhs, U &&rhs) = delete;

        // && and || are not supported because of operator precedence
        // use parenthesis instead of ||
        template<class U>
        friend
        binary_operands<T, U const &>
        operator||(first_operand &&lhs, U &&rhs) = delete;

        [[nodiscard]]
        std::string
        format() const
        {
            return format_value(lhs_);
        }
    };

    // Converts the first elements in the expression to a first_operand wrapper
    // first_operand wrapper will then wrap the other elements in the expression
    // These wrappers allow the application to have access to all elements in an
    // expression and evaluate them as needed to generate proper error messages
    struct decomposer
    {
        template<class T>
        friend
        first_operand<T const&>
        operator<=(decomposer &&, T &&lhs)
        {
            return first_operand<T const &>{lhs};
        }
    };
}

/*
 * Macros to suppress -Wparentheses warnings
 *
 * These warnings are suppressed because they are triggered by macros
 * including extra parentheses to avoid errors related to operator
 * precedence.
 */
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

#endif //MRDOCS_TEST_DECOMPOSER_HPP
