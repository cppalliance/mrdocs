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

template <class F>
class ScopeExit {
    F onExitScope_;
    bool dismissed_{false};
public:
    explicit ScopeExit(F onExitScope)
        : onExitScope_(std::move(onExitScope)) {}

    ~ScopeExit() {
        if (!dismissed_) {
            onExitScope_();
        }
    }

    void
    dismiss() {
        dismissed_ = true;
    }
};

template <class F>
ScopeExit(F) -> ScopeExit<F>;

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

    ~ScopeExitRestore()
    {
        if (!dismissed_) {
            ref_ = prev_;
        }
    }

    void
    dismiss() {
        dismissed_ = true;
    }
};

template <class T>
ScopeExitRestore(T&) -> ScopeExitRestore<T>;

template <class T, std::convertible_to<T> T2>
ScopeExitRestore(T&, T2) -> ScopeExitRestore<T>;

}

#endif // MRDOCS_API_SUPPORT_SCOPEEXIT_HPP