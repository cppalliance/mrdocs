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

namespace clang::mrdocs {

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
    explicit
    ScopeExitRestore(T& ref)
        : prev_(ref), ref_(ref)
    {}

    explicit
    ScopeExitRestore(T& ref, T next)
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

template <class T>
ScopeExitRestore(T&, T) -> ScopeExitRestore<T>;

}

#endif // MRDOCS_API_SUPPORT_SCOPEEXIT_HPP