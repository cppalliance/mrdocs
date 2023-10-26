//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef MRDOCS_TEST_HPP
#define MRDOCS_TEST_HPP

#if defined(_MSC_VER)
# pragma once
#endif

//#include <boost/current_function.hpp>
#include <cctype>
#include <sstream>
#include <type_traits>
#include "detail/decomposer.hpp"

//  This is a derivative work
//  Copyright 2002-2018 Peter Dimov
//  Copyright (c) 2002, 2009, 2014 Peter Dimov
//  Copyright (2) Beman Dawes 2010, 2011
//  Copyright (3) Ion Gaztanaga 2013
//
//  Copyright 2018 Glen Joseph Fernandes
//  (glenjofe@gmail.com)

namespace test_suite {

//------------------------------------------------

struct any_suite
{
    virtual ~any_suite() = 0;
    virtual char const* name() const noexcept = 0;
    virtual void run() const = 0;
};

//------------------------------------------------

struct suites
{
    virtual ~suites() = default;

    using iterator = any_suite const* const*;
    virtual void insert(any_suite const&) = 0;
    virtual iterator begin() const noexcept = 0;
    virtual iterator end() const noexcept = 0;

    // DEPRECATED
    virtual void sort() = 0;

    static suites& instance() noexcept;
};

//------------------------------------------------

template<class T>
class suite : public any_suite
{
    char const* name_;

public:
    explicit
    suite(char const* name) noexcept
        : name_(name)
    {
        suites::instance().insert(*this);
    }

    char const*
    name() const noexcept override
    {
        return name_;
    }

    void
    run() const override
    {
        T().run();
    }
};

//------------------------------------------------

class any_runner
{
    any_runner* prev_;

    static any_runner*&
    instance_impl() noexcept;

public:
    static any_runner& instance() noexcept;

    any_runner() noexcept;
    virtual ~any_runner();

    virtual void run(any_suite const& test) = 0;
    virtual void note(char const* msg) = 0;
    virtual bool test(bool cond,
        char const* expr, char const* func,
        char const* file, int line) = 0;
    virtual std::ostream& log() noexcept = 0;
};

//------------------------------------------------

namespace detail {

// In the comparisons below, it is possible that
// T and U are signed and unsigned integer types,
// which generates warnings in some compilers.
// A cleaner fix would require common_type trait
// or some meta-programming, which would introduce
// a dependency on Boost.TypeTraits. To avoid
// the dependency we just disable the warnings.
#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wsign-compare")
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-compare"
# endif
#elif defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4389)
# pragma warning(disable: 4018)
#elif defined(__GNUC__) && !(defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 406
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

template<class...>
struct make_void
{
    using type = void;
};

template<class... Ts>
using void_t = typename make_void<Ts...>::type;

// Whether wchar_t is signed is implementation-defined
template<bool Signed> struct lwt_long_type {};
template<> struct lwt_long_type<true> { typedef long type; };
template<> struct lwt_long_type<false> { typedef unsigned long type; };
inline lwt_long_type<
    (static_cast<wchar_t>(-1) < static_cast<wchar_t>(0))
        >::type test_output_impl( wchar_t const& v ) { return v; }

#if !defined( BOOST_NO_CXX11_CHAR16_T )
inline unsigned long test_output_impl( char16_t const& v ) { return v; }
#endif

#if !defined( BOOST_NO_CXX11_CHAR32_T )
inline unsigned long test_output_impl( char32_t const& v ) { return v; }
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

inline std::string test_output_impl( char const& v )
{
    if( std::isprint( static_cast<unsigned char>( v ) ) )
    {
        return { 1, v };
    }
    else
    {
        char buffer[ 8 ];
        std::sprintf( buffer, "\\x%02X", static_cast<unsigned char>( v ) );

        return buffer;
    }
}

bool
test_impl(
    bool cond,
    char const* expr,
    char const* func,
    char const* file,
    int line);

void
throw_failed_impl(
    const char* expr,
    char const* excep,
    char const* func,
    char const* file,
    int line);

void
no_throw_failed_impl(
    const char* expr,
    char const* excep,
    char const* func,
    char const* file,
    int line);

void
no_throw_failed_impl(
    const char* expr,
    char const* func,
    char const* file,
    int line);

//------------------------------------------------

#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wsign-compare")
#  pragma clang diagnostic pop
# endif
#elif defined(_MSC_VER)
# pragma warning(pop)
#elif defined(__GNUC__) && !(defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 406
# pragma GCC diagnostic pop
#endif

//------------------------------------------------

struct log_type
{
    template<class T>
    friend
    std::ostream&
    operator<<(
        log_type const&, T&& t)
    {
        return any_runner::instance().log() << t;
    }
};

//------------------------------------------------

} // detail

/** Log output to the current suite
*/
constexpr detail::log_type log{};

#define DETAIL_STRINGIFY(...) #__VA_ARGS__

#define BOOST_TEST(...)                                          \
  [&] {                                                          \
  if (!(static_cast<bool>(__VA_ARGS__)))                         \
  {                                                              \
      DETAIL_START_WARNINGS_SUPPRESSION                          \
      std::string d = DETAIL_STRINGIFY(__VA_ARGS__);             \
      d += "\n    (";                                            \
      DETAIL_SUPPRESS_PARENTHESES_WARNINGS                       \
      d += (test_suite::detail::decomposer() <= __VA_ARGS__).format(); \
      DETAIL_STOP_WARNINGS_SUPPRESSION                           \
      d += ")";                                                  \
      return ::test_suite::detail::test_impl(                    \
          false,                                                 \
          d.data(),                                              \
          "@anon",                                               \
          __FILE__,                                              \
          __LINE__ );                                            \
  } else {                                                       \
      return ::test_suite::detail::test_impl(                    \
          true,                                                  \
          DETAIL_STRINGIFY(__VA_ARGS__),                         \
          "@anon",                                               \
          __FILE__,                                              \
          __LINE__ );                                            \
  }                                                              \
  }()

#define BOOST_ERROR(msg) \
    ::test_suite::detail::test_impl( \
        false, msg, "@anon", __FILE__, __LINE__ )

#define BOOST_TEST_EQ(expr1,expr2) \
    BOOST_TEST( (expr1) == (expr2) )

#define BOOST_TEST_CSTR_EQ(expr1,expr2) \
    BOOST_TEST( string_view(expr1) == string_view(expr2) )

#define BOOST_TEST_NE(expr1,expr2) \
    BOOST_TEST( (expr1) != (expr2) )

#define BOOST_TEST_LT(expr1,expr2) \
    BOOST_TEST( (expr1) < (expr2) )

#define BOOST_TEST_LE(expr1,expr2) \
    BOOST_TEST( (expr1) <= (expr2) )

#define BOOST_TEST_GT(expr1,expr2) \
    BOOST_TEST( (expr1) > (expr2) )

#define BOOST_TEST_GE(expr1,expr2) \
    BOOST_TEST ( (expr1) >= (expr2) )

#define BOOST_TEST_PASS() BOOST_TEST(true)

#define BOOST_TEST_FAIL() BOOST_TEST(false)

#define BOOST_TEST_NOT(expr) BOOST_TEST(!(expr))

#ifndef BOOST_NO_EXCEPTIONS
# define BOOST_TEST_THROW_WITH( expr, ex, msg ) \
    try { \
        (void)(expr); \
        ::test_suite::detail::throw_failed_impl( \
            #expr, #ex, "@anon", \
            __FILE__, __LINE__); \
    } \
    catch(ex const& e) {                 \
        BOOST_TEST(std::string_view(e.what()) == std::string_view(msg)); \
    } \
    catch(...) { \
        ::test_suite::detail::throw_failed_impl( \
            #expr, #ex, "@anon", \
            __FILE__, __LINE__); \
    }                            \
    (void) 0
   //

# define BOOST_TEST_THROW_STARTS_WITH( expr, ex, msg ) \
    try { \
        (void)(expr); \
        ::test_suite::detail::throw_failed_impl( \
            #expr, #ex, "@anon", \
            __FILE__, __LINE__); \
    } \
    catch(ex const& e) {                 \
        BOOST_TEST(std::string_view(e.what()).starts_with(std::string_view(msg))); \
    } \
    catch(...) { \
        ::test_suite::detail::throw_failed_impl( \
            #expr, #ex, "@anon", \
            __FILE__, __LINE__); \
    }                            \
    (void) 0
   //

# define BOOST_TEST_THROWS( expr, ex ) \
    try { \
        (void)(expr); \
        ::test_suite::detail::throw_failed_impl( \
            #expr, #ex, "@anon", \
            __FILE__, __LINE__); \
    } \
    catch(ex const&) {                 \
        BOOST_TEST_PASS(); \
    } \
    catch(...) { \
        ::test_suite::detail::throw_failed_impl( \
            #expr, #ex, "@anon", \
            __FILE__, __LINE__); \
    }                            \
    (void) 0
   //

# define BOOST_TEST_NO_THROW( expr ) \
    try { \
        (void)(expr); \
        BOOST_TEST_PASS(); \
    } catch (const std::exception& e) { \
        ::test_suite::detail::no_throw_failed_impl( \
            #expr, e.what(), "@anon", \
            __FILE__, __LINE__); \
    } catch (...) { \
        ::test_suite::detail::no_throw_failed_impl( \
            #expr, "@anon", \
            __FILE__, __LINE__); \
    }
    //
#else
   #define BOOST_TEST_THROWS( expr, ex )
   #define BOOST_TEST_THROW_WITH( expr, ex, msg )
   #define BOOST_TEST_NO_THROW( expr ) ( [&]{ expr; return true; }() )
#endif

#define TEST_SUITE(type, name) \
    static ::test_suite::suite<type> type##_(name)

extern int unit_test_main(int argc, char const* const* argv);

} // test_suite

#endif
