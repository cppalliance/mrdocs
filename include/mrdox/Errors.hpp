//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_ERRORS_HPP
#define MRDOX_ERRORS_HPP

#include <mrdox/detail/access.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <cassert>
#include <new>
#include <source_location>
#include <system_error>
#include <type_traits>

namespace clang {
namespace mrdox {

//------------------------------------------------

// VFALCO CLEAN UP THIS HACK!
namespace detail {
CONST_FUNCTION_ACCESS(callGetPtr, llvm::Error, getPtr, llvm::ErrorInfoBase*);
} // detail

//------------------------------------------------
//
// ErrorCode
//
//------------------------------------------------

/** Holds a portable error code
*/
class ErrorCode
{
    std::error_code ec_;
    std::source_location loc_;

    static
    llvm::ErrorInfoBase const*
    getPtr(llvm::Error const& e) noexcept
    {
        return access::callFunction<detail::callGetPtr>(e);
    }

    static
    std::error_code
    getErrorCode(
        llvm::Error const& e) noexcept
    {
        auto const p = getPtr(e);
        if(! p)
            return std::error_code();
        return p->convertToErrorCode();
    }

public:
    ErrorCode() noexcept = default;
    ErrorCode(ErrorCode const&) noexcept = default;
    ErrorCode& operator=(ErrorCode const&) noexcept = default;

    ErrorCode(
        llvm::Error& e,
        std::source_location const& loc = 
            std::source_location::current()) noexcept
        : ec_(getErrorCode(e))
        , loc_(loc)
    {
        e.operator bool();
    }

    ErrorCode(
        llvm::Error&& e,
        std::source_location const& loc = 
            std::source_location::current()) noexcept
        : ec_(getErrorCode(e))
        , loc_(loc)
    {
        llvm::Error e_(std::move(e));
        e_.operator bool();
    }

    std::string
    message() const
    {
        return ec_.message();
    }

    std::source_location const&
    where() const noexcept
    {
        return loc_;
    }

    bool failed() const noexcept
    {
        return ec_.operator bool();
    }

    operator bool() const noexcept
    {
        return failed();
    }

    [[noreturn]]
    void throwFrom(
        std::source_location const& loc =
            std::source_location::current()) const;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        ErrorCode const& ec)
    {
        ec.write(os);
        return os;
    }

private:
    void write(llvm::raw_ostream& os) const;
};

//------------------------------------------------
//
// Result
//
//------------------------------------------------

/** A variant which holds a return value or an ErrorCode.
*/
template<class T>
class Result
{
public:
    using pointer = std::remove_reference_t<T> *;
    using reference = std::remove_reference_t<T> &;
    using const_pointer = std::remove_reference_t<T> const*;
    using const_reference = std::remove_reference_t<T> const&;

    /** Destructor.
    */
    ~Result()
    {
        if(hasError_)
            ec_.~ErrorCode();
        else
            t_.~T();
    }

    /** Constructor.
    */
    template<
        bool B = std::is_default_constructible_v<T>,
        std::enable_if_t<B, int> = 0>
    Result()
        : hasError_(false)
    {
        new(&t_) T();
    }

    /** Constructor.
    */
    Result(
        Result&& other)
    {
        construct(std::move(other));
    }

    /** Constructor.
    */
    template<class U>
    explicit
    Result(
        Result<U>&& other,
        std::enable_if_t<! std::is_convertible_v<U, T>>* = 0)
    {
        construct(std::move(other));
    }

    /** Constructor.
    */
    template<class U>
    Result(
        Result<U>&& other,
        std::enable_if_t<std::is_convertible_v<U, T>>* = 0)
    {
        construct(std::move(other));
    }

    /** Constructor.
    */
    template<class U>
    Result(
        U&& u,
        std::enable_if_t<std::is_convertible_v<U, T>>* = 0)
        : hasError_(false)
    {
        new(&t_) T(std::forward<U>(u));
    }

    /** Constructor.
    */
    Result(
        ErrorCode const& ec) noexcept
        : hasError_(true)
    {
        new(&ec_) ErrorCode(ec);
    }

    /** Constructor.
    */
    template<class T>
    Result(llvm::Expected<T>&& ex)
    {
        construct(std::move(ex));
    }

    /** Assignment.
    */
    Result&
    operator=(Result&& other)
    {
        assign(std::move(other));
        return *this;
    }

    bool hasValue() const noexcept
    {
        return ! hasError_;
    }

    bool hasError() const noexcept
    {
        return hasError_;
    }

    explicit operator bool() const noexcept
    {
        return ! hasError_;
    }

    T& value(
        std::source_location const& loc =
            std::source_location::current()) &
    {
        if(! hasError_)
            return t_;
        ec_.throwFrom(loc);
    }

    T const& value(
        std::source_location const& loc =
            std::source_location::current()) const&
    {
        if(! hasError_)
            return t_;
        ec_.throwFrom(loc);
    }

    template<class U = T>
    std::enable_if_t<
        std::is_move_constructible_v<U>, T>
    value(
        std::source_location const& loc =
            std::source_location::current()) &&
    {
        return std::move(value(loc));
    }

    template<class U = T>
    std::enable_if_t<
        ! std::is_move_constructible_v<U>, T&&>
    value(
        std::source_location const& loc =
            std::source_location::current()) &&
    {
        return std::move(value(loc));
    }

    template<class U = T>
    std::enable_if_t<
        std::is_move_constructible_v<U>, T>
    value(
        std::source_location const& loc =
            std::source_location::current()) const&& = delete;

    template<class U = T>
    std::enable_if_t<
        ! std::is_move_constructible_v<U>, T const&&>
    value(
        std::source_location const& loc =
            std::source_location::current()) const&&
    {
        return std::move(value(loc));
    }

    T* operator->() noexcept
    {
        return &t_;
    }

    T const* operator->() const noexcept
    {
        return &t_;
    }

    T& operator*() & noexcept
    {
        T* p = operator->();
        assert(p != nullptr);
        return *p;
    }

    T const& operator*() const & noexcept
    {
        T const* p = operator->();
        assert(p != nullptr);
        return *p;
    }

    template<class U = T>
    std::enable_if_t<std::is_move_constructible_v<U>, T>
    operator*() && noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        return std::move(**this);
    }

    template<class U = T>
    std::enable_if_t<! std::is_move_constructible_v<U>, T&&>
    operator*() && noexcept
    {
        return std::move(**this);
    }

    template<class U = T>
    std::enable_if_t<std::is_move_constructible_v<U>, T>
    operator*() const && noexcept = delete;

    template<class U = T>
    std::enable_if_t<! std::is_move_constructible_v<U>, T const&&>
    operator*() const&& noexcept
    {
        return std::move(**this);
    }

    ErrorCode error() const noexcept
    {
        return hasError_ ? ec_ : ErrorCode();
    }

    template<class... Args>
    T& emplace(Args&&... args)
    {
        if(hasError_)
        {
            struct undo
            {
                Result& r_;
                ErrorCode ec_;
                bool commit_ = false;
                explicit undo(Result& r) noexcept
                    : r_(r)
                    , ec_(r.ec_)
                {
                    r_.ec_.~ErrorCode();
                }

                ~undo()
                {
                    if(commit_)
                        return;
                    r_.ec_ = ec_;
                    r_.hasError_ = true;
                }

                void commit() noexcept
                {
                    commit_ = true;
                }
            };
            undo u(*this);
            new(&t_) T(std::forward<Args>(args)...);
            u.commit();
            hasError_ = false;
        }
        else
        {
            t_ = T(std::forward<Args>(args)...);
        }
    }

    friend bool operator==(
        Result const& r0, Result const& r1 ) noexcept
    {
        if(r0.hasError())
        {
            if(r1.hasError())
                return r0.error() == r1.error();
            return false;
        }
        if(! r1.hasError())
            return *r0 == *r1;
        return false;
    }

    friend bool operator!=(
        Result const& r0, Result const& r1 ) noexcept
    {
        return !(r0 == r1);
    }

private:
    template<class U>
    static bool isAlias(
        U const& u0, U const& u1) noexcept
    {
        return &u0 == &u1;
    }

    template<class T_, class U>
    static bool isAlias(
        T_ const&, U const&) noexcept
    {
        return false;
    }

    template<class U>
    void construct(
        Result<U>&& other)
    {
        hasError_ = other.hasError_;
        if(! hasError_)
            new(&t_) T(std::move(other.t_));
        else
            new(&ec_) ErrorCode(std::move(other.ec_));
    }

    template<class U>
    void construct(
        llvm::Expected<U>&& other)
    {
        if(! other.operator bool())
        {
            hasError_ = true;
            new(&ec_) ErrorCode(other.takeError());
        }
        else
        {
            hasError_ = false;
            new(&t_) T(std::move(other.get()));
        }
    }

    template<class U>
    void assign(Result<U>&& other)
    {
        if(isAlias(*this, other))
            return;
        // VFALCO This depends on `other` not throwing on move
        this->~Result();
        new(this) Result(std::move(other));
    }

    union
    {
        T t_;
        ErrorCode ec_;
    };

    bool hasError_ = false;
};

template<class T>
Result(llvm::Expected<T> &&) -> Result<T>;

//------------------------------------------------
//
// Reporter
//
//------------------------------------------------

/** Used to check and report errors uniformly.
*/
struct Reporter
{
    template<class T>
    void
    fail(
        llvm::StringRef what,
        Result<T> const& res)
    {
        if(res.hasError())
            fail(what, res.error());
    }

    void
    fail(
        llvm::StringRef what,
        ErrorCode const& ec);

    //--------------------------------------------
    // VFALCO GARBAGE BELOW THIS LINE

    bool failed()
    {
        return failed_;
    }

    void test_failure()
    {
        failed_ = true;
    }

    bool success(llvm::Error&& err);
    bool success(std::error_code const& ec);

    template<class T>
    bool
    success(T& t, llvm::ErrorOr<T>&& ev)
    {
        if(ev)
        {
            t = std::move(ev.get());
            return true;
        }
        return false;
    }

    bool
    success(
        llvm::StringRef what,
        std::error_code const& ec);

    bool
    success(
        llvm::StringRef what,
        llvm::Error& err);

private:
    bool failed_ = false;
};

} // mrdox
} // clang

#endif
