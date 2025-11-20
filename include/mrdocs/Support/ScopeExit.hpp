//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_SCOPEEXIT_HPP
#define MRDOCS_API_SUPPORT_SCOPEEXIT_HPP

#include <utility>

namespace mrdocs {

/** Execute a callable when the enclosing scope exits unless dismissed.
*/
template <class F>
class ScopeExit {
    F onExitScope_;
    bool dismissed_{false};
public:
    /** Construct with a callable to invoke on scope exit.

        @param onExitScope Callable executed unless dismissed.
    */
    explicit ScopeExit(F onExitScope)
        : onExitScope_(std::move(onExitScope)) {}

    /** Invoke the stored callable if the guard was not dismissed.
    */
    ~ScopeExit() {
        if (!dismissed_) {
            onExitScope_();
        }
    }

    /** Prevent the callable from running on destruction.
    */
    void
    dismiss() {
        dismissed_ = true;
    }
};

/** Deduction guide for ScopeExit.
*/
template <class F>
ScopeExit(F) -> ScopeExit<F>;

/** RAII helper that restores a referenced value on scope exit.
*/
template <class T>
class ScopeExitRestore {
    T prev_;
    T& ref_;
    bool dismissed_{false};
public:
    /** Restore `ref` to its previous value when the scope ends

        Store the current value of `ref` and restore it
        when this object goes out of scope, unless `dismiss()`
        is called.
    */
    explicit
    ScopeExitRestore(T& ref)
        : prev_(ref), ref_(ref)
    {}

    /** Temporarily set `ref` to `next` and restore it when the scope ends

        @param ref The variable to modify and eventually restore.
        @param next The temporary value assigned to `ref` for the scope.

        Store the current value of `ref`, set it to `next`,
        and restore the previous value when this object goes
        out of scope, unless `dismiss()` is called.
    */
    template <std::convertible_to<T> T2>
    explicit
    ScopeExitRestore(T& ref, T2 next)
        : prev_(ref), ref_(ref)
    {
        ref_ = next;
    }

    /** Restore the previous value unless dismissed.
    */
    ~ScopeExitRestore()
    {
        if (!dismissed_) {
            ref_ = prev_;
        }
    }

    /** Prevent restoration on destruction.
    */
    void
    dismiss() {
        dismissed_ = true;
    }
};

/** Deduction guide for ScopeExitRestore taking a reference.
*/
template <class T>
ScopeExitRestore(T&) -> ScopeExitRestore<T>;

/** Deduction guide for ScopeExitRestore taking a reference and new value.
*/
template <class T, std::convertible_to<T> T2>
ScopeExitRestore(T&, T2) -> ScopeExitRestore<T>;

}

#endif // MRDOCS_API_SUPPORT_SCOPEEXIT_HPP
