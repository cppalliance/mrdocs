//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

//
// JerryScript-backed JavaScript bridge for MrDocs
//
// Architecture Overview
// ---------------------
//
// This module provides a C++ interface to JerryScript, enabling JavaScript
// execution for Handlebars template helpers. The design supports M:N
// threading: any number of Context objects (interpreters) can be used by
// any number of threads, with proper synchronization.
//
// Key Components:
//
// - Context: Owns an isolated JerryScript interpreter with its own 512KB
//   heap. Multiple Contexts can exist simultaneously—the count is not
//   limited by thread count. Each Context has a mutex for thread-safe
//   access; a thread activates a Context before performing operations,
//   then releases it for other threads to use.
//
// - Scope: Provides RAII-style value tracking within a Context. When a
//   Scope is destroyed, it releases references to values created within
//   it. Values that were copied elsewhere (returned, stored) survive;
//   values that remained local are freed. This provides deterministic
//   cleanup similar to stack-based scripting engines.
//
// - Value: Handle to a JavaScript value. Internally stores a jerry_value_t
//   (as uint32_t) plus a shared_ptr to the owning Context. Before any
//   JerryScript operation, the Value locks and activates its Context,
//   ensuring thread safety and correct TLS state.
//
// Threading Model:
//
// JerryScript is single-threaded per context, but we can have multiple
// contexts. Thread-local storage (TLS) tracks which context is currently
// active on each thread. When a thread needs to use a Context:
//
//   1. Lock the Context's mutex (serializes access to that interpreter)
//   2. Set TLS to point to that Context's interpreter
//   3. Perform JerryScript operations
//   4. Release the lock (TLS may still point there; that's fine)
//
// This allows patterns like:
//   - 4 threads sharing 4 Contexts (1:1, maximum parallelism)
//   - 4 threads sharing 100 Contexts (threads switch between contexts)
//   - 1 thread using multiple Contexts sequentially
//
// DOM Conversion:
//
// - DOM → JS (toJsValue): Objects use lazy Proxy wrappers to avoid
//   infinite recursion from circular references (e.g., Handlebars symbol
//   contexts). Arrays are converted eagerly. Functions wrap dom::Function.
//
// - JS → DOM (toDomValue): Proxies unwrap to their original dom::Value.
//   JS functions become callable from C++. Arrays/objects convert
//   recursively.
//

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Assert.hpp>
#include <mrdocs/Support/Handlebars.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <jerryscript.h>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <pthread.h>
#endif

// ------------------------------------------------------------
// JerryScript External Context Port Functions
// ------------------------------------------------------------
//
// JerryScript with JERRY_EXTERNAL_CONTEXT=ON requires the host to provide
// three port functions for context management:
//
//   - jerry_port_context_alloc: Allocates memory for context + heap
//   - jerry_port_context_free:  Frees context memory
//   - jerry_port_context_get:   Returns the currently active context
//
// The default jerry-port implementations use a single static global pointer,
// limiting the entire process to one interpreter. Our implementations use
// thread-local storage (TLS) to track which context is active on each thread,
// enabling the M:N threading model described above.
//
// Important: TLS stores the *currently active* context, not a per-thread
// context. A thread activates whichever context it needs to work with;
// multiple contexts can exist and any thread can use any context (one at
// a time per context, enforced by the mutex).
//
// The context port functions are excluded from jerry-port when building
// with JERRY_EXTERNAL_CONTEXT=ON (see third-party/patches/jerryscript/
// CMakeLists.txt), so mrdocs provides the only implementations. All other
// port functions (jerry_port_fatal, jerry_port_log, etc.) use the default
// implementations from jerry-port.

// ------------------------------------------------------------
// Thread-Local Storage for JerryScript Context
// ------------------------------------------------------------
//
// We use POSIX pthread TLS on non-Windows platforms instead of C++ thread_local
// because GCC with static linking (-static) has known issues with C++ thread_local
// variables accessed from extern "C" functions. The pthread TLS API is more
// portable and works reliably with static linking.
//
// On Windows, we use C++ thread_local which works correctly with MSVC.

#ifdef _WIN32
// Windows: use C++ thread_local (works correctly with MSVC)
static thread_local void* tls_jerry_context = nullptr;
static thread_local bool tls_context_alloc_failed = false;

static void* get_tls_jerry_context() { return tls_jerry_context; }
static void set_tls_jerry_context(void* ptr) { tls_jerry_context = ptr; }
static bool get_tls_context_alloc_failed() { return tls_context_alloc_failed; }
static void set_tls_context_alloc_failed(bool val) { tls_context_alloc_failed = val; }

#else
// POSIX: use pthread TLS for compatibility with static linking on Linux/GCC

// TLS keys for context pointer and allocation failure flag
static pthread_key_t tls_jerry_context_key;
static pthread_key_t tls_context_alloc_failed_key;
static pthread_once_t tls_keys_init_once = PTHREAD_ONCE_INIT;

static void init_tls_keys()
{
    pthread_key_create(&tls_jerry_context_key, nullptr);
    pthread_key_create(&tls_context_alloc_failed_key, nullptr);
}

static void ensure_tls_keys_initialized()
{
    pthread_once(&tls_keys_init_once, init_tls_keys);
}

static void* get_tls_jerry_context()
{
    ensure_tls_keys_initialized();
    return pthread_getspecific(tls_jerry_context_key);
}

static void set_tls_jerry_context(void* ptr)
{
    ensure_tls_keys_initialized();
    pthread_setspecific(tls_jerry_context_key, ptr);
}

static bool get_tls_context_alloc_failed()
{
    ensure_tls_keys_initialized();
    // Use pointer value as bool (nullptr = false, non-null = true)
    return pthread_getspecific(tls_context_alloc_failed_key) != nullptr;
}

static void set_tls_context_alloc_failed(bool val)
{
    ensure_tls_keys_initialized();
    // Store bool as pointer (nullptr = false, (void*)1 = true)
    pthread_setspecific(tls_context_alloc_failed_key, val ? (void*)1 : nullptr);
}
#endif

// Heap size per context. 512KB is JerryScript's typical maximum when built
// with 16-bit compressed pointers (JERRY_CPOINTER_32_BIT=OFF).
static constexpr std::size_t JERRY_HEAP_SIZE = 512 * 1024;

// Allocates memory for a new JerryScript context and its heap.
// Called internally by jerry_init(). The returned block contains the context
// structure followed by JERRY_HEAP_SIZE bytes for the JavaScript heap.
// Temporarily stores the pointer in TLS so jerry_port_context_get() works
// during initialization; Context::Impl captures it and restores TLS afterward.
extern "C" void*
jerry_port_context_alloc(jerry_size_t context_size)
{
    // Allocate context structure + heap in one contiguous block.
    // JerryScript uses the excess space beyond context_size as the JS heap.
    std::size_t total_size = context_size + JERRY_HEAP_SIZE;

    // aligned_alloc on glibc requires the size to be a multiple of the
    // alignment. Round up to satisfy that requirement to avoid
    // heap-corruption crashes (observed as munmap_chunk/free() errors with
    // GCC static builds).
    std::size_t const align = alignof(std::max_align_t);
    if (std::size_t const rem = total_size % align)
    {
        total_size += align - rem;
    }

    // Use aligned allocation for proper pointer alignment
    void* ptr = nullptr;
#if defined(_MSC_VER)
    ptr = _aligned_malloc(total_size, alignof(std::max_align_t));
#else
    ptr = std::aligned_alloc(alignof(std::max_align_t), total_size);
#endif
    if (!ptr)
    {
        // Signal allocation failure via TLS flag. The Context::Impl constructor
        // will check this flag and throw a C++ exception for graceful error handling.
        // We return nullptr here; JerryScript may fail, but Context::Impl will
        // detect the failure before any operations are attempted.
        set_tls_context_alloc_failed(true);
        return nullptr;
    }

    // Store in TLS so jerry_port_context_get() returns this during jerry_init().
    // The Context::Impl constructor will capture this and restore previous TLS.
    set_tls_jerry_context(ptr);

    return ptr;
}

// Frees context memory. Called internally by jerry_cleanup().
// JerryScript declares this as jerry_port_context_free(void) — no parameters.
// The implementation must retrieve the context pointer itself (via TLS).
extern "C" void
jerry_port_context_free(void)
{
    void* ctx = get_tls_jerry_context();
    if (!ctx) // LCOV_EXCL_LINE
        return; // LCOV_EXCL_LINE
    // MSVC's jerry_port_context_alloc uses _aligned_malloc,
    // which requires _aligned_free (std::free is undefined behavior).
#if defined(_MSC_VER)
    _aligned_free(ctx);
#else
    std::free(ctx);
#endif
    set_tls_jerry_context(nullptr);
}

// Returns the currently active context for this thread.
// Called by JerryScript before every operation to find the interpreter state.
// Returns nullptr if no context is active (which would cause JerryScript to crash).
extern "C" struct jerry_context_t*
jerry_port_context_get(void)
{
    return static_cast<jerry_context_t*>(get_tls_jerry_context());
}

namespace mrdocs::js {

namespace detail {

// Validate Handlebars-style helper arguments: options object must be last.
// Returns an error if options are missing/invalid; otherwise calls the helper.
// For simple helpers (those with only primitive arguments), we strip the
// options object before calling JavaScript to avoid expensive/recursive
// conversion of symbol contexts.
Expected<dom::Value, Error>
invokeHelper(Value const& fn, dom::Array const& args)
{
    if (args.empty())
    {
        return Unexpected(Error(
            "Handlebars helper called without arguments; "
            "expected options object as last argument"));
    }

    dom::Value const& options = args.back();
    if (!options.isObject())
    {
        return Unexpected(Error(
            "Handlebars helper options must be an object; "
            "ensure the helper is called from a template context"));
    }

    // Build arguments without the options object.
    // JavaScript helpers typically don't need Handlebars options (hash, fn,
    // inverse, context) - they just operate on positional arguments.
    // Passing the options object would trigger expensive recursive conversion
    // of symbol contexts which contain circular references.
    std::vector<dom::Value> callArgs;
    callArgs.reserve(args.size() - 1);
    for (std::size_t i = 0; i < args.size() - 1; ++i)
    {
        callArgs.push_back(args.get(i));
    }

    auto ret = fn.apply(callArgs);
    if (!ret)
    {
        return Unexpected(ret.error());
    }
    return ret->getDom();
}

} // namespace detail

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------

// Convert a JerryScript value to UTF-8, never throwing; used for diagnostics.
// Diagnostic-only: stringifies any value (including exceptions) to owned UTF-8
// and returns "<error>" if JerryScript itself throws during stringification.
static std::string
toString(jerry_value_t v)
{
    jerry_value_t str = jerry_value_to_string(v);
    if (jerry_value_is_exception(str))
    {
        jerry_value_free(str);
        return "<error>";
    }
    jerry_size_t sz = jerry_string_size(str, JERRY_ENCODING_UTF8);
    std::string out(sz, '\0');
    jerry_string_to_buffer(
        str,
        JERRY_ENCODING_UTF8,
        (jerry_char_t*) out.data(),
        sz);
    jerry_value_free(str);
    return out;
}

// Normalize a JerryScript exception into a MrDocs Error type.
// Order: unwrap exception → if object use .message → else if string use it →
// otherwise stringify the original exception.
//
// Error message format:
// - Syntax errors from JerryScript typically contain "Unexpected" or "SyntaxError"
// - Runtime errors (thrown exceptions) are prefixed with "Unexpected: " if they
//   don't already contain that marker, helping distinguish them from parse errors
// - This prefix is intentionally consistent to aid debugging and testing
//
// LIMITATION: The "Unexpected" heuristic isn't perfect - some runtime errors
// may contain "Unexpected" in their message and won't get the prefix, while
// some custom syntax-like errors might get prefixed incorrectly. This is
// acceptable because the prefix is for debugging convenience, not semantic
// correctness.
static Error
makeError(jerry_value_t exc)
{
    jerry_value_t obj = jerry_value_is_exception(exc) ?
                            jerry_exception_value(exc, false) :
                            jerry_value_copy(exc);

    std::string msg;
    if (jerry_value_is_object(obj))
    {
        // Note: jerry_string_sz is used here instead of makeString because
        // makeString is defined later in this file and we need to extract
        // error messages early in the error handling path.
        jerry_value_t msg_key = jerry_string_sz("message");
        jerry_value_t msg_prop = jerry_object_get(obj, msg_key);
        jerry_value_free(msg_key);
        if (!jerry_value_is_exception(msg_prop))
        {
            msg = toString(msg_prop);
        }
        jerry_value_free(msg_prop);
    }
    else if (jerry_value_is_string(obj))
    {
        msg = toString(obj);
    }

    if (msg.empty() || msg == "undefined")
    {
        msg = toString(exc);
    }

    // Prefix runtime exceptions for consistent error messaging. Skip if the
    // message already indicates a syntax/parse error (contains "Unexpected")
    // or if this isn't actually an exception value.
    if (jerry_value_is_exception(exc)
        && msg.find("Unexpected") == std::string::npos)
    {
        msg = std::string("Unexpected: ") + msg;
    }

    jerry_value_free(obj);
    return Error(msg.empty() ? "JavaScript error" : msg);
}

// Forward declarations for conversion utilities used by Scope/Value methods
static dom::Value
toDomValue(jerry_value_t v, std::shared_ptr<Context::Impl> const& impl);

static jerry_value_t
toJsValue(dom::Value const& v, std::shared_ptr<Context::Impl> const& impl);

// Base class for native holders used by proxies/functions.
struct NativeHolder {
    virtual ~NativeHolder() = default;
};

// Common holder structure for lazy proxies. Stores the original dom::Value
// so it can be retrieved when converting back from JS to DOM.
struct DomValueHolder : NativeHolder {
    std::shared_ptr<Context::Impl> impl;
    dom::Value value;  // The original DOM value (Object or Array)

    // free_cb defined after Context::Impl to access unregisterHolder
    static void free_cb(void* p, jerry_object_native_info_t*);
};

// Single native info for all DOM value proxies, allowing detection in type()
// and toDomValue. Defined later after all forward declarations are complete.
extern jerry_object_native_info_t const kDomProxyInfo;

static std::string_view
trimLeftSpaces(std::string_view sv);

// Forward declarations for helpers referenced by Scope
static std::string
escapeForEval(std::string_view src);

static jerry_value_t
makeString(std::string_view s);

static jerry_value_t
to_js(std::uint32_t v);

static std::uint32_t
to_handle(jerry_value_t v);

// ------------------------------------------------------------
// Context
// ------------------------------------------------------------

// Per-context state: owns an isolated JerryScript interpreter instance.
// Contexts are thread-affine: they are created and used on the same thread,
// but a thread may create multiple contexts if desired.
struct Context::Impl {
    // Opaque pointer to JerryScript context memory (context struct + heap).
    // Allocated by jerry_port_context_alloc, freed by jerry_port_context_free.
    void* jerry_ctx = nullptr;

    // Thread that most recently used this context (for debug diagnostics).
    mutable std::thread::id owner_thread{};

    // Lifetime flag so deleters can skip freeing after cleanup.
    bool alive = true;

    // Flag set while cleanup/jerry_cleanup is running to suppress deleters.
    bool cleaning_up = false;

    // Serialize access to this JerryScript context (single-threaded engine).
    mutable std::recursive_mutex mtx;

    // Optional diagnostics: track live JS handles we create (Value copies etc).
    std::atomic<int> live_handles{0};

    // Track all native holders (DomValueHolder, FunctionHolder) so we can
    // delete them during cleanup if JerryScript's GC doesn't finalize them.
    // This handles the case where objects are still referenced from globals.
    std::unordered_set<NativeHolder*> holders;

    void registerHolder(NativeHolder* h)
    {
        holders.insert(h);
    }

    void unregisterHolder(NativeHolder* h)
    {
        holders.erase(h);
    }

    Impl()
    {
        // Temporarily set TLS so jerry_init() can find the context.
        // jerry_init() calls jerry_port_context_alloc() internally.
        // We need a two-phase init: first allocate, then init.
        //
        // Actually, jerry_init() itself calls jerry_port_context_alloc(),
        // so we set TLS *after* the allocation returns and before jerry_init()
        // uses the context. The trick is jerry_port_context_get() is called
        // *during* jerry_init() after allocation.
        //
        // Approach: jerry_init() allocates via jerry_port_context_alloc(),
        // stores the pointer internally, then calls jerry_port_context_get()
        // for subsequent operations. We capture the allocated pointer.

        // For external context, jerry_init behavior:
        // 1. Calls jerry_port_context_alloc() to get memory
        // 2. Stores pointer and calls jerry_port_context_get() for future ops
        //
        // We need to ensure jerry_port_context_get() returns the right pointer.
        // Since jerry_init() doesn't give us the pointer back directly,
        // we use a temporary TLS approach during init.

        // Clear any previous allocation failure flag
        set_tls_context_alloc_failed(false);

        // Set a sentinel so we know init is in progress
        void* prev_ctx = get_tls_jerry_context();

        // During jerry_init(), JerryScript will:
        // 1. Call jerry_port_context_alloc() - we allocate and save in TLS
        // 2. Call jerry_port_context_get() - returns our TLS value
        jerry_init(JERRY_INIT_EMPTY);

        // Check if allocation failed during jerry_init()
        if (get_tls_context_alloc_failed())
        {
            set_tls_context_alloc_failed(false);
            set_tls_jerry_context(prev_ctx);
            throw std::bad_alloc();
        }

        // After init, TLS contains the allocated context
        jerry_ctx = get_tls_jerry_context();

        // Restore previous TLS (likely nullptr)
        set_tls_jerry_context(prev_ctx);
    }

    ~Impl()
    {
        // cleanup() should have been called before destruction.
        // If not (e.g., Context was moved from), just clean up the context.
        if (jerry_ctx)
        {
            cleanup();
        }
    }

    // Tear down the JerryScript context.
    // Must run on the owning thread.
    void cleanup()
    {
        if (!jerry_ctx)
            return;

        cleaning_up = true;
        // Activate this context for cleanup. jerry_cleanup() uses TLS
        // (via jerry_port_context_get) to find the context to tear down.
        void* prev_ctx = get_tls_jerry_context();
        set_tls_jerry_context(jerry_ctx);

        // Optional optimization: run GC to finalize unreferenced objects and
        // trigger their free_cb callbacks, which unregister them from our
        // holders set. Objects still referenced (e.g., globals) won't be
        // collected here but will be handled by the manual cleanup loop below.
        jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);

        // jerry_cleanup() tears down JS objects and, with JERRY_EXTERNAL_CONTEXT=ON,
        // calls jerry_port_context_free() to release the context memory.
        jerry_cleanup();

        // Delete any remaining native holders that weren't garbage collected.
        // This handles objects still referenced from globals at cleanup time.
        // The free_cb won't be called for these since JerryScript just abandons
        // them during cleanup, so we delete them manually.
        for (NativeHolder* h : holders)
        {
            delete h;
        }
        holders.clear();

        // Context is now destroyed. Set jerry_ctx to nullptr and mark dead.
        jerry_ctx = nullptr;
        alive = false;
        cleaning_up = false;

        // Restore previous TLS since the context is now destroyed.
        set_tls_jerry_context(prev_ctx);
    }

    // Activate this context on the current thread.
    // Must be called before any JerryScript operations.
    void activate() const
    {
        owner_thread = std::this_thread::get_id();
        set_tls_jerry_context(jerry_ctx);
    }

};

// DomValueHolder free callback - defined here after Context::Impl is complete.
void DomValueHolder::free_cb(void* p, jerry_object_native_info_t*)
{
    auto* h = static_cast<DomValueHolder*>(p);
    // Always unregister from tracking set so we don't double-free during cleanup.
    if (h->impl)
    {
        h->impl->unregisterHolder(h);
    }
    delete h;
}

// Activate the context for the current thread. RAII restores previous TLS value
// and releases the mutex lock when destroyed.
struct ContextActivation {
    std::shared_ptr<Context::Impl> impl;
    std::optional<std::unique_lock<std::recursive_mutex>> lock;
    jerry_context_t* prev_ctx{};

    explicit ContextActivation(std::shared_ptr<Context::Impl> const& i)
        : impl(i)
    {
        if (!impl)
            return;
        // Acquire mutex lock BEFORE activating the context
        lock.emplace(impl->mtx);
        prev_ctx = static_cast<jerry_context_t*>(get_tls_jerry_context());
        impl->activate();
    }

    // Non-copyable to prevent double-restore of TLS
    ContextActivation(ContextActivation const&) = delete;
    ContextActivation& operator=(ContextActivation const&) = delete;

    // Move constructor - transfers ownership of lock and TLS restoration duty
    ContextActivation(ContextActivation&& other) noexcept
        : impl(std::move(other.impl))
        , lock(std::move(other.lock))
        , prev_ctx(other.prev_ctx)
    {
        // other.impl is now nullptr, so its destructor won't restore TLS
    }

    // Move assignment
    ContextActivation& operator=(ContextActivation&& other) noexcept
    {
        if (this != &other)
        {
            // Restore our prev_ctx before taking other's state
            if (impl)
            {
                set_tls_jerry_context(prev_ctx);
            }
            impl = std::move(other.impl);
            lock = std::move(other.lock);
            prev_ctx = other.prev_ctx;
        }
        return *this;
    }

    ~ContextActivation()
    {
        if (impl)
        {
            set_tls_jerry_context(prev_ctx);
        }
        // lock is automatically released when destroyed (after TLS restore)
    }

    explicit operator bool() const { return static_cast<bool>(impl); }
};

static ContextActivation
lockContext(std::shared_ptr<Context::Impl> const& impl)
{
    // Accepts null shared_ptr so callers can use it uniformly in move/copy
    // paths where the source Value may have been moved-from (val_ == 0).
    // The ContextActivation constructor handles the null case.
    return ContextActivation(impl);
}

Context::Context() : impl_(std::make_shared<Impl>()) {}

Context::Context(Context const& other) noexcept = default;

Context::~Context()
{
    // Clean up the JerryScript context before releasing impl_.
    // DomValueHolder objects keep shared_ptr<Impl>, so cleanup breaks that cycle.
    if (impl_)
    {
        impl_->cleanup();
    }
}

// ------------------------------------------------------------
// Scope
// ------------------------------------------------------------

Scope::Scope(Context const& ctx) noexcept : impl_(ctx.impl_)
{
}

Scope::~Scope()
{
    auto lock = lockContext(impl_);

    // Release one reference to each tracked value.
    // Values that were copied elsewhere survive (refcount > 1).
    // Values that remained local are freed (refcount == 1).
    for (std::uint32_t v : tracked_)
    {
        jerry_value_free(to_js(v));
    }
    tracked_.clear();
}

Value
Scope::pushInteger(std::int64_t v)
{
    auto lock = lockContext(impl_);
    jerry_value_t jv = jerry_number(static_cast<double>(v));
    if (jerry_value_is_exception(jv))
    {
        report::warn("JavaScript: failed to create integer value");
        jerry_value_free(jv);
        return {};
    }
    tracked_.push_back(to_handle(jv));  // Scope holds one ref
    return {to_handle(jerry_value_copy(jv)), impl_};  // Value gets its own ref
}

Value
Scope::pushDouble(double v)
{
    auto lock = lockContext(impl_);
    jerry_value_t jv = jerry_number(v);
    if (jerry_value_is_exception(jv))
    {
        report::warn("JavaScript: failed to create double value");
        jerry_value_free(jv);
        return {};
    }
    tracked_.push_back(to_handle(jv));
    return {to_handle(jerry_value_copy(jv)), impl_};
}

Value
Scope::pushBoolean(bool v)
{
    auto lock = lockContext(impl_);
    jerry_value_t jv = jerry_boolean(v);
    if (jerry_value_is_exception(jv))
    {
        report::warn("JavaScript: failed to create boolean value");
        jerry_value_free(jv);
        return {};
    }
    tracked_.push_back(to_handle(jv));
    return {to_handle(jerry_value_copy(jv)), impl_};
}

Value
Scope::pushString(std::string_view v)
{
    auto lock = lockContext(impl_);
    jerry_value_t jv = makeString(v);
    if (jerry_value_is_exception(jv))
    {
        report::warn("JavaScript: failed to create string value");
        jerry_value_free(jv);
        return {};
    }
    tracked_.push_back(to_handle(jv));
    return {to_handle(jerry_value_copy(jv)), impl_};
}

Value
Scope::pushObject()
{
    auto lock = lockContext(impl_);
    jerry_value_t jv = jerry_object();
    if (jerry_value_is_exception(jv))
    {
        report::warn("JavaScript: failed to create object");
        jerry_value_free(jv);
        return {};
    }
    tracked_.push_back(to_handle(jv));
    return {to_handle(jerry_value_copy(jv)), impl_};
}

Value
Scope::pushArray()
{
    auto lock = lockContext(impl_);
    jerry_value_t jv = jerry_array(0);
    if (jerry_value_is_exception(jv))
    {
        report::warn("JavaScript: failed to create array");
        jerry_value_free(jv);
        return {};
    }
    tracked_.push_back(to_handle(jv));
    return {to_handle(jerry_value_copy(jv)), impl_};
}

Expected<Value, Error>
Scope::eval(std::string_view script)
{
    auto lock = lockContext(impl_);
    // Values from eval are transferred to caller, not tracked by Scope.
    jerry_value_t res = jerry_eval(
        (jerry_char_t const*) script.data(),
        script.size(),
        JERRY_PARSE_NO_OPTS);
    if (jerry_value_is_exception(res))
    {
        auto err = makeError(res);
        jerry_value_free(res);
        return Unexpected(err);
    }
    return Value(to_handle(res), impl_);
}

Expected<void>
Scope::script(std::string_view jsCode)
{
    auto exp = eval(jsCode);
    if (!exp)
    {
        return Unexpected(exp.error());
    }
    return {};
}

Expected<Value, Error>
Scope::compile_script(std::string_view script)
{
    // KNOWN LIMITATION: This implementation uses manual string matching and
    // eval-based wrapping, which is fragile (false positives on "function" in
    // strings/comments, escaping issues). A proper solution requires a more
    // thoughtful design that considers:
    // - How users import/require other modules
    // - Whether to support ES modules (import/export)
    // - How to handle multi-file helper libraries
    //
    // Turn an arbitrary script into a callable that can be executed later. We
    // reject bare function declarations (which JerryScript treats as script
    // statements) and wrap the source in an IIFE returning the eval result so
    // callers get a function they can invoke repeatedly.
    auto trimmed = trimLeftSpaces(script);
    if (trimmed.starts_with("function"))
    {
        return Unexpected(Error("script contains a function declaration"));
    }

    // Build a function that defers evaluation until invocation and returns the
    // eval result
    std::string wrapper = "(function(){ return eval(\"";
    wrapper.append(escapeForEval(script));
    wrapper.append("\"); })");

    auto exp = eval(wrapper);
    if (!exp)
    {
        return Unexpected(exp.error());
    }
    if (!exp->isFunction())
    {
        return Unexpected(Error("compiled script is not a function"));
    }
    return *exp;
}

Expected<Value, Error>
Scope::compile_function(std::string_view script)
{
    // KNOWN LIMITATION: This implementation uses manual string parsing to find
    // function names, which is fragile:
    // - "function" in strings/comments causes false positives
    // - Arrow functions and async functions aren't detected
    // - Class methods aren't supported
    // - Trial-and-error execution may cause side effects
    //
    // A proper solution requires a more thoughtful design that considers:
    // - How users import/require other modules
    // - Whether to support ES modules (import/export)
    // - How to handle multi-file helper libraries
    //
    // Current approach: First try parenthesizing to force expression parsing;
    // if that fails, execute the script and search for the first "function"
    // keyword to extract the declared function name.
    //
    // SIDE EFFECTS WARNING: If the parenthesized expression attempt fails
    // (e.g., for scripts with statements before the function), the fallback
    // path executes the script to define the function. Scripts like:
    //   "counter++; function foo() {}"
    // will increment counter during compile_function even though the intent
    // is only to extract the function.
    //
    // Parenthesize the provided source so it is treated as a function expression
    std::string wrapped = "(";
    wrapped.append(script);
    wrapped.append(")");
    auto exp = eval(wrapped);
    if (exp && exp->isFunction())
    {
        return *exp;
    }

    // Fall back: execute declarations and return the first declared function
    // name. Note: this path runs the script, so any side effects will occur.
    auto findFirstFunctionName =
        [](std::string_view sv) -> std::optional<std::string> {
        std::size_t pos = 0;
        while (true)
        {
            pos = sv.find("function", pos);
            if (pos == std::string_view::npos)
            {
                return std::nullopt;
            }
            pos += 8;
            auto nameView = trimLeftSpaces(sv.substr(pos));
            std::size_t const wsSkipped = nameView.data() - sv.data() - pos;
            std::size_t start = pos + wsSkipped;
            std::size_t cur = start;
            while (cur < sv.size()
                   && (std::isalnum(static_cast<unsigned char>(sv[cur]))
                       || sv[cur] == '_' || sv[cur] == '$'))
            {
                ++cur;
            }
            if (start != cur)
            {
                return std::string(sv.substr(start, cur - start));
            }
        }
    };

    auto name = findFirstFunctionName(script);
    if (!name)
    {
        return Unexpected(Error("code did not evaluate to a function"));
    }

    std::string builder = "(function(){\n";
    builder.append(script);
    builder.append("\nreturn ");
    builder.append(*name);
    builder.append(";\n})()");

    auto exec = eval(builder);
    if (!exec)
    {
        return Unexpected(exec.error());
    }
    if (!exec->isFunction())
    {
        return Unexpected(Error("code did not evaluate to a function"));
    }
    return *exec;
}

void
Scope::setGlobal(std::string_view name, dom::Value const& value)
{
    auto lock = lockContext(impl_);
    jerry_value_t realm = jerry_current_realm();
    jerry_value_t global = jerry_realm_this(realm);
    jerry_value_t k = makeString(name);
    jerry_value_t v = toJsValue(value, impl_);
    jerry_value_t res = jerry_object_set(global, k, v);
    jerry_value_free(k);
    jerry_value_free(v);
    jerry_value_free(res);
    jerry_value_free(global);
    jerry_value_free(realm);
}

Expected<Value, Error>
Scope::getGlobal(std::string_view name)
{
    auto lock = lockContext(impl_);
    // Returned value is transferred to caller, not tracked.
    jerry_value_t realm = jerry_current_realm();
    jerry_value_t global = jerry_realm_this(realm);
    jerry_value_t k = makeString(name);
    jerry_value_t v = jerry_object_get(global, k);
    jerry_value_free(global);
    jerry_value_free(realm);
    jerry_value_free(k);
    if (jerry_value_is_exception(v))
    {
        auto err = makeError(v);
        jerry_value_free(v);
        return Unexpected(err);
    }
    return Value(to_handle(v), impl_);
}

Value
Scope::getGlobalObject()
{
    auto lock = lockContext(impl_);
    // Returned value is transferred to caller, not tracked.
    jerry_value_t realm = jerry_current_realm();
    jerry_value_t g = jerry_realm_this(realm);
    jerry_value_free(realm);
    return {to_handle(g), impl_};
}

// ------------------------------------------------------------
// Value
// ------------------------------------------------------------

// Helpers to round-trip raw JerryScript handles through our opaque Value
// storage without reinterpreting the bits elsewhere. ABI guard: fails at
// build-time if a future JerryScript changes jerry_value_t size/representation.
static jerry_value_t
to_js(std::uint32_t v)
{
    return static_cast<jerry_value_t>(v);
}

static std::uint32_t
to_handle(jerry_value_t v)
{
    return static_cast<std::uint32_t>(v);
}

static_assert(
    std::is_same<std::uint32_t, jerry_value_t>::value,
    "jerry_value_t size mismatch");

Value::Value() noexcept : val_(0) {}

Value::Value(std::uint32_t val, std::shared_ptr<Context::Impl> impl) noexcept
    : impl_(std::move(impl))
    , val_(val)
{}

Value::~Value()
{
    // Only free the value if the context is still valid.
    // After Context::cleanup(), jerry_ctx is nullptr and we can't call
    // JerryScript functions. Values that outlive their Context (e.g.,
    // captured in lambdas) will skip cleanup - the memory was already
    // freed by jerry_cleanup().
    // IMPORTANT: We must check jerry_ctx INSIDE the lock to avoid TOCTOU race
    // with cleanup() which sets jerry_ctx = nullptr under the same lock.
    if (val_ && impl_)
    {
        auto lock = lockContext(impl_);
        if (impl_->jerry_ctx)
        {
            jerry_value_free(to_js(val_));
        }
    }
}

Value::Value(Value const& other) : impl_(other.impl_), val_(0)
{
    // Copy by bumping JerryScript handle refcount; paired with jerry_value_free
    // in the destructor for shared lifetime management across Value copies.
    //
    // Thread safety note: The shared_ptr copy (impl_) is done outside the lock
    // because std::shared_ptr is thread-safe for concurrent copies. The
    // jerry_value_copy call requires the lock since JerryScript is single-threaded.
    // This allows Values to be safely copied across threads while ensuring all
    // engine operations are serialized.
    //
    // Skip copy if context has been cleaned up - the value can't be used anyway.
    // IMPORTANT: We must check jerry_ctx INSIDE the lock to avoid TOCTOU race
    // with cleanup() which sets jerry_ctx = nullptr under the same lock.
    if (other.val_ && other.impl_)
    {
        auto lock = lockContext(other.impl_);
        if (other.impl_->jerry_ctx)
        {
            val_ = to_handle(jerry_value_copy(to_js(other.val_)));
        }
    }
}

Value::Value(Value&& other) noexcept
    : impl_(std::move(other.impl_))
    , val_(other.val_)
{
    other.val_ = 0;
}

Value&
Value::operator=(Value const& other)
{
    if (this == &other)
    {
        return *this;
    }
    // Free old value if context is still valid
    // IMPORTANT: We must check jerry_ctx INSIDE the lock to avoid TOCTOU race
    // with cleanup() which sets jerry_ctx = nullptr under the same lock.
    if (val_ && impl_)
    {
        auto lock = lockContext(impl_);
        if (impl_->jerry_ctx)
        {
            jerry_value_free(to_js(val_));
        }
    }
    impl_ = other.impl_;
    // Copy new value if context is still valid
    if (other.val_ && other.impl_)
    {
        auto lock = lockContext(other.impl_);
        if (other.impl_->jerry_ctx)
        {
            val_ = to_handle(jerry_value_copy(to_js(other.val_)));
        }
        else
        {
            val_ = 0;
        }
    }
    else
    {
        val_ = 0;
    }
    return *this;
}

Value&
Value::operator=(Value&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    // Free old value if context is still valid
    // IMPORTANT: We must check jerry_ctx INSIDE the lock to avoid TOCTOU race
    // with cleanup() which sets jerry_ctx = nullptr under the same lock.
    if (val_ && impl_)
    {
        auto lock = lockContext(impl_);
        if (impl_->jerry_ctx)
        {
            jerry_value_free(to_js(val_));
        }
    }
    impl_ = std::move(other.impl_);
    val_ = other.val_;
    other.val_ = 0;
    return *this;
}

void
Value::swap(Value& other) noexcept
{
    using std::swap;
    swap(impl_, other.impl_);
    swap(val_, other.val_);
}

static bool
isSafeNumberForJerry(double d)
{
    // JerryScript only guarantees 32-bit ints; reject wider values early to
    // avoid wraparound in the engine and round-trip surprises.
    // Note: std::isfinite returns false for NaN and ±Infinity, so those are
    // correctly rejected here without needing a separate std::isnan check.
    if (!std::isfinite(d))
    {
        return false;
    }
    constexpr auto kMin = static_cast<double>(
        std::numeric_limits<std::int32_t>::min());
    constexpr auto kMax = static_cast<double>(
        std::numeric_limits<std::int32_t>::max());
    return d >= kMin && d <= kMax;
}

static std::string
escapeForEval(std::string_view src)
{
    std::string out;
    out.reserve(src.size() + 16);
    for (char c: src)
    {
        switch (c)
        {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    return out;
}

static std::string_view
trimLeftSpaces(std::string_view sv)
{
    while (!sv.empty()
           && std::isspace(static_cast<unsigned char>(sv.front())))
    {
        sv.remove_prefix(1);
    }
    return sv;
}

static jerry_value_t
makeString(std::string_view s)
{
    // Create a JerryScript UTF-8 string from a std::string_view without
    // leaking ownership details to callers. JerryScript replaces invalid
    // sequences with U+FFFD; inputs are expected to be UTF-8.
    return jerry_string(
        reinterpret_cast<jerry_char_t const*>(s.data()),
        static_cast<jerry_size_t>(s.size()),
        JERRY_ENCODING_UTF8);
}

Type
Value::type() const noexcept
{
    if (!val_)
    {
        return Type::undefined;
    }
    auto lock = lockContext(impl_);
    auto v = to_js(val_);
    if (jerry_value_is_undefined(v))
    {
        return Type::undefined;
    }
    if (jerry_value_is_null(v))
    {
        return Type::null;
    }
    if (jerry_value_is_boolean(v))
    {
        return Type::boolean;
    }
    if (jerry_value_is_number(v))
    {
        return Type::number;
    }
    if (jerry_value_is_string(v))
    {
        return Type::string;
    }
    if (jerry_value_is_function(v))
    {
        return Type::function;
    }
    if (jerry_value_is_array(v))
    {
        return Type::array;
    }
    // Check if this is one of our DOM object proxies - if so, return object.
    // (Arrays are converted eagerly, so they're real JS arrays, not proxies.)
    if (jerry_value_is_proxy(v))
    {
        jerry_value_t handler = jerry_proxy_handler(v);
        if (!jerry_value_is_exception(handler))
        {
            // Native pointer is stored directly on the handler object.
            auto* holder = static_cast<DomValueHolder*>(
                jerry_object_get_native_ptr(handler, &kDomProxyInfo));
            if (holder)
            {
                jerry_value_free(handler);
                // DOM object proxies wrap objects only (arrays are eager)
                return Type::object;
            }
        }
        jerry_value_free(handler);
    }
    return Type::object;
}


bool
Value::isTruthy() const noexcept
{
    if (!val_)
    {
        return false;
    }
    auto lock = lockContext(impl_);
    return jerry_value_to_boolean(to_js(val_));
}

dom::Value
Value::getDom() const
{
    if (!val_)
    {
        return nullptr;
    }
    return toDomValue(to_js(val_), impl_);
}

std::string
Value::getString() const
{
    return std::string(getDom().getString());
}

bool
Value::getBool() const noexcept
{
    MRDOCS_ASSERT(isBoolean());
    return getDom().getBool();
}

std::int64_t
Value::getInteger() const noexcept
{
    MRDOCS_ASSERT(isNumber());
    auto lock = lockContext(impl_);
    double d = jerry_value_as_number(to_js(val_));
    if (d >= (double) std::numeric_limits<std::int64_t>::max())
    {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (d <= (double) std::numeric_limits<std::int64_t>::min())
    {
        return std::numeric_limits<std::int64_t>::min();
    }
    return static_cast<std::int64_t>(d);
}

double
Value::getDouble() const noexcept
{
    MRDOCS_ASSERT(isNumber());
    auto lock = lockContext(impl_);
    return jerry_value_as_number(to_js(val_));
}

dom::Object
Value::getObject() const noexcept
{
    return getDom().getObject();
}

dom::Array
Value::getArray() const noexcept
{
    return getDom().getArray();
}

dom::Function
Value::getFunction() const noexcept
{
    return getDom().getFunction();
}

bool
Value::isInteger() const noexcept
{
    if (!isNumber())
    {
        return false;
    }
    double d = getDouble();
    auto i = static_cast<std::int64_t>(d);
    return static_cast<double>(i) == d;
}

bool
Value::isDouble() const noexcept
{
    return isNumber() && !isInteger();
}

Value
Value::get(std::size_t i) const
{
    if (!isArray())
    {
        return {};
    }
    auto lock = lockContext(impl_);
    jerry_value_t arr = to_js(val_);
    jerry_value_t v = jerry_object_get_index(arr, (uint32_t) i);
    if (jerry_value_is_exception(v))
    {
        jerry_value_free(v);
        return {};
    }
    return {to_handle(v), impl_};
}

Value
Value::get(dom::Value const& idx) const
{
    if (idx.isString())
    {
        return get(idx.getString());
    }
    if (idx.isInteger())
    {
        return get((std::size_t) idx.getInteger());
    }
    return {};
}

Value
Value::lookup(std::string_view keys) const
{
    Value cur = *this;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= keys.size(); ++i)
    {
        if (i == keys.size() || keys[i] == '.')
        {
            std::string_view token = keys.substr(start, i - start);
            cur = cur.get(token);
            start = i + 1;
        }
    }
    return cur;
}

void
Value::erase(std::string_view key) const
{
    if (!isObject())
    {
        return;
    }
    auto lock = lockContext(impl_);
    jerry_value_t obj = to_js(val_);
    jerry_value_t k = makeString(key);
    jerry_value_t r = jerry_object_delete(obj, k);
    jerry_value_free(r);
    jerry_value_free(k);
}

bool
Value::exists(std::string_view key) const
{
    // Fast-path array indices without allocating JerryScript strings; otherwise
    // defer to property lookup. This mirrors JS truthiness while avoiding
    // exceptions for missing elements.
    if (isArray())
    {
        // If key is an unsigned integer index, query the array directly without
        // allocating or throwing.
        uint32_t idx = 0;
        bool allDigits = !key.empty();
        for (char c: key)
        {
            if (c < '0' || c > '9')
            {
                allDigits = false;
                break;
            }
            idx = idx * 10 + static_cast<uint32_t>(c - '0');
        }
        if (allDigits)
        {
            auto lock = lockContext(impl_);
            jerry_value_t elem = jerry_object_get_index(val_, idx);
            bool exists = !jerry_value_is_exception(elem)
                          && !jerry_value_is_undefined(elem);
            jerry_value_free(elem);
            return exists;
        }
    }
    if (!isObject())
    {
        return false;
    }
    auto lock = lockContext(impl_);
    jerry_value_t obj = to_js(val_);
    jerry_value_t k = makeString(key);
    jerry_value_t res = jerry_object_has(obj, k);
    bool b = jerry_value_to_boolean(res);
    jerry_value_free(res);
    jerry_value_free(k);
    return b;
}

bool
Value::empty() const
{
    auto sz = size();
    return sz == 0;
}

std::size_t
Value::size() const
{
    // Approximate JS length semantics: arrays report their length property,
    // objects return key count, strings return byte length, numbers/booleans
    // count as singletons, and other types report zero.
    if (isArray())
    {
        auto lock = lockContext(impl_);
        jerry_value_t lenKey = makeString("length");
        jerry_value_t lenVal = jerry_object_get(to_js(val_), lenKey);
        jerry_value_free(lenKey);
        if (jerry_value_is_exception(lenVal))
        {
            jerry_value_free(lenVal);
            return 0;
        }
        std::size_t len = (std::size_t) jerry_value_as_number(lenVal);
        jerry_value_free(lenVal);
        return len;
    }
    if (isObject())
    {
        auto lock = lockContext(impl_);
        jerry_value_t keys = jerry_object_keys(val_);
        std::size_t len = (std::size_t) jerry_array_length(keys);
        jerry_value_free(keys);
        return len;
    }
    if (isString())
    {
        return getString().size();
    }
    if (isNumber() || isBoolean())
    {
        return 1;
    }
    return 0;
}

Value
Value::operator[](std::string_view key) const
{
    return get(key);
}

Value
Value::operator[](std::size_t index) const
{
    return get(index);
}

void
Value::set(std::string_view name, Value const& value) const
{
    if (!val_)
    {
        return;
    }
    auto lock = lockContext(impl_);
    jerry_value_t obj = to_js(val_);
    jerry_value_t k = makeString(name);
    jerry_value_t v = jerry_value_copy(to_js(value.val_));
    jerry_value_t res = jerry_object_set(obj, k, v);
    jerry_value_free(k);
    jerry_value_free(v);
    jerry_value_free(res);
}

void
Value::set(std::string_view key, dom::Value const& value) const
{
    Value v = Value(to_handle(toJsValue(value, impl_)), impl_);
    set(key, v);
}

Value
Value::get(std::string_view name) const
{
    if (!val_)
    {
        return {};
    }
    auto lock = lockContext(impl_);
    jerry_value_t obj = to_js(val_);
    jerry_value_t k = makeString(name);
    jerry_value_t v = jerry_object_get(obj, k);
    jerry_value_free(k);
    if (jerry_value_is_exception(v))
    {
        jerry_value_free(v);
        return {};
    }
    return Value(to_handle(v), impl_);
}

Expected<Value>
Value::apply(std::span<dom::Value const> args) const
{
    // Shared call path for Function invocations so wrappers (`apply`,
    // Handlebars helpers, etc.) consistently marshal DOM values into
    // JerryScript values, call the engine, then convert back or surface an
    // exception as Error.
    if (!val_)
    {
        return Unexpected(Error("undefined"));
    }
    auto lock = lockContext(impl_);
    jerry_value_t fn = val_;
    if (!jerry_value_is_function(fn))
    {
        return Unexpected(Error("not a function"));
    }

    std::vector<jerry_value_t> jsArgs;
    jsArgs.reserve(args.size());
    for (auto const& a: args)
    {
        jsArgs.push_back(toJsValue(a, impl_));
    }

    jerry_value_t ret
        = jerry_call(fn, jerry_undefined(), jsArgs.data(), jsArgs.size());
    for (auto& a: jsArgs)
    {
        jerry_value_free(a);
    }
    if (jerry_value_is_exception(ret))
    {
        auto err = makeError(ret);
        jerry_value_free(ret);
        return Unexpected(err);
    }
    return Value(to_handle(ret), impl_);
}

// ------------------------------------------------------------
// dom <-> JS conversion
// ------------------------------------------------------------

// Definition of kDomProxyInfo (declared earlier as extern)
jerry_object_native_info_t const kDomProxyInfo{ DomValueHolder::free_cb, 0, 0 };

// Retrieve the DomValueHolder from a proxy trap's handler object.
// Returns nullptr if the holder is not found or invalid.
static DomValueHolder*
getHolderFromHandler(jerry_value_t thisValue)
{
    // The native pointer is stored directly on the handler object.
    return static_cast<DomValueHolder*>(
        jerry_object_get_native_ptr(thisValue, &kDomProxyInfo));
}

// ------------------------------------------------------------
// Lazy Object Proxy
// ------------------------------------------------------------
// Creates a JavaScript Proxy that wraps a dom::Object. Properties are
// converted lazily when accessed, avoiding infinite recursion from circular
// references (e.g., symbols that reference parent symbols in Handlebars
// options objects).

static jerry_value_t
makeObjectProxy(dom::Object obj, std::shared_ptr<Context::Impl> impl)
{
    auto* holder = new DomValueHolder();
    holder->impl = impl;
    holder->value = dom::Value(std::move(obj));
    impl->registerHolder(holder);

    // Create an empty target object (the proxy intercepts all access)
    jerry_value_t target = jerry_object();

    // Create handler object with traps
    jerry_value_t handler = jerry_object();

    // 'get' trap: handler.get(target, prop, receiver)
    jerry_value_t get_fn = jerry_function_external(
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_undefined();
            auto* h = getHolderFromHandler(call_info_p->this_value);
            if (!h)
                return jerry_undefined();

            std::string propName = toString(args_p[1]);
            auto lock = lockContext(h->impl);
            dom::Value val = h->value.getObject().get(propName);
            return toJsValue(val, h->impl);
        });

    jerry_value_t get_key = makeString("get");
    jerry_value_t sr = jerry_object_set(handler, get_key, get_fn);
    jerry_value_free(sr);
    jerry_value_free(get_key);
    jerry_value_free(get_fn);

    // 'has' trap: handler.has(target, prop)
    jerry_value_t has_fn = jerry_function_external(
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_boolean(false);
            auto* h = getHolderFromHandler(call_info_p->this_value);
            if (!h)
                return jerry_boolean(false);

            std::string propName = toString(args_p[1]);
            auto lock = lockContext(h->impl);
            return jerry_boolean(h->value.getObject().exists(propName));
        });

    jerry_value_t has_key = makeString("has");
    sr = jerry_object_set(handler, has_key, has_fn);
    jerry_value_free(sr);
    jerry_value_free(has_key);
    jerry_value_free(has_fn);

    // 'ownKeys' trap: handler.ownKeys(target)
    jerry_value_t ownKeys_fn = jerry_function_external(
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const[],
           jerry_length_t) -> jerry_value_t
        {
            auto* h = getHolderFromHandler(call_info_p->this_value);
            if (!h)
                return jerry_array(0);

            auto lock = lockContext(h->impl);
            std::vector<std::string> keys;
            h->value.getObject().visit([&](dom::String k, dom::Value const&) {
                keys.push_back(std::string(k.get()));
                return true;
            });

            jerry_value_t arr = jerry_array(keys.size());
            for (uint32_t i = 0; i < keys.size(); ++i)
            {
                jerry_value_t keyVal = makeString(keys[i]);
                jerry_value_t setRes = jerry_object_set_index(arr, i, keyVal);
                jerry_value_free(setRes);
                jerry_value_free(keyVal);
            }
            return arr;
        });

    jerry_value_t ownKeys_key = makeString("ownKeys");
    sr = jerry_object_set(handler, ownKeys_key, ownKeys_fn);
    jerry_value_free(sr);
    jerry_value_free(ownKeys_key);
    jerry_value_free(ownKeys_fn);

    // 'getOwnPropertyDescriptor' trap (needed for ownKeys to work properly)
    jerry_value_t getOwnPropDesc_fn = jerry_function_external(
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_undefined();
            auto* h = getHolderFromHandler(call_info_p->this_value);
            if (!h)
                return jerry_undefined();

            std::string propName = toString(args_p[1]);
            auto lock = lockContext(h->impl);
            if (!h->value.getObject().exists(propName))
                return jerry_undefined();

            // Return a property descriptor
            jerry_value_t desc = jerry_object();
            jerry_value_t val = toJsValue(h->value.getObject().get(propName), h->impl);
            jerry_value_t setRes;

            jerry_value_t valueKey = makeString("value");
            setRes = jerry_object_set(desc, valueKey, val);
            jerry_value_free(setRes);
            jerry_value_free(valueKey);
            jerry_value_free(val);

            jerry_value_t writableKey = makeString("writable");
            setRes = jerry_object_set(desc, writableKey, jerry_boolean(true));
            jerry_value_free(setRes);
            jerry_value_free(writableKey);

            jerry_value_t enumKey = makeString("enumerable");
            setRes = jerry_object_set(desc, enumKey, jerry_boolean(true));
            jerry_value_free(setRes);
            jerry_value_free(enumKey);

            jerry_value_t configKey = makeString("configurable");
            setRes = jerry_object_set(desc, configKey, jerry_boolean(true));
            jerry_value_free(setRes);
            jerry_value_free(configKey);

            return desc;
        });

    jerry_value_t getOwnPropDesc_key = makeString("getOwnPropertyDescriptor");
    sr = jerry_object_set(handler, getOwnPropDesc_key, getOwnPropDesc_fn);
    jerry_value_free(sr);
    jerry_value_free(getOwnPropDesc_key);
    jerry_value_free(getOwnPropDesc_fn);

    // Store the holder directly on the handler object via native pointer.
    // When the handler is garbage collected (after the proxy is collected),
    // DomValueHolder::free_cb will be called to delete the holder.
    jerry_object_set_native_ptr(handler, &kDomProxyInfo, holder);

    // Create the proxy
    jerry_value_t proxy = jerry_proxy(target, handler);
    jerry_value_free(target);
    jerry_value_free(handler);  // proxy now owns handler (and its native pointer)

    // If proxy creation fails, handler was still freed above, which triggers
    // free_cb to delete the holder. Return empty object.
    if (jerry_value_is_exception(proxy))
    {
        jerry_value_free(proxy);
        return jerry_object();
    }

    return proxy;
}

// Holder for wrapped dom::Function, inherits NativeHolder for cleanup tracking.
struct FunctionHolder : NativeHolder {
    std::shared_ptr<Context::Impl> impl;
    dom::Function fn;

    static void
    free_cb(void* p, jerry_object_native_info_t*)
    {
        auto* h = static_cast<FunctionHolder*>(p);
        // Always unregister from tracking set so we don't double-free during cleanup.
        if (h->impl)
        {
            h->impl->unregisterHolder(h);
        }
        delete h;
    }
};

static jerry_object_native_info_t const kFunctionHolderInfo{ FunctionHolder::free_cb, 0, 0 };

static jerry_value_t
makeFunctionProxy(dom::Function fn, std::shared_ptr<Context::Impl> impl)
{
    // Wrap a Dom::Function so JerryScript can call it while keeping the native
    // callable alive via a heap-allocated holder.
    auto* holder = new FunctionHolder();
    holder->impl = impl;
    holder->fn = std::move(fn);
    impl->registerHolder(holder);

    jerry_value_t func = jerry_function_external(
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) {
        auto* h = static_cast<FunctionHolder*>(
            jerry_object_get_native_ptr(call_info_p->function, &kFunctionHolderInfo));
        if (!h)
        {
            return jerry_throw_sz(JERRY_ERROR_COMMON, "no function");
        }
        if (h->impl->owner_thread != std::this_thread::get_id())
        {
            return jerry_throw_sz(JERRY_ERROR_COMMON, "function called on wrong thread");
        }
        auto lock = lockContext(h->impl);
        dom::Array arr;
        for (jerry_length_t i = 0; i < argc; ++i)
        {
            arr.push_back(toDomValue(args_p[i], h->impl));
        }
        auto exp = h->fn.call(arr);
        if (!exp)
        {
            return jerry_throw_sz(
                JERRY_ERROR_COMMON,
                exp.error().message().c_str());
        }
        return toJsValue(*exp, h->impl);
    });

    jerry_object_set_native_ptr(func, &kFunctionHolderInfo, holder);
    return func;
}

static jerry_value_t
toJsValue(dom::Value const& v, std::shared_ptr<Context::Impl> const& impl)
{
    // Convert a DOM value tree into JerryScript heap objects. Objects and
    // arrays are wrapped in Proxies for lazy conversion - properties/elements
    // are only converted when accessed. This avoids infinite recursion from
    // circular references (e.g., symbols that reference parent symbols in
    // Handlebars options objects) and improves performance by not converting
    // properties that are never used.
    auto lock = lockContext(impl);
    switch (v.kind())
    {
    case dom::Kind::Null:
        return jerry_null();
    case dom::Kind::Boolean:
        return jerry_boolean(v.getBool());
    case dom::Kind::Integer:
    {
        // JerryScript (3.0.0) narrows through int32 fast-path; large values
        // trip UBSan.
        auto i = v.getInteger();
        if (!isSafeNumberForJerry(static_cast<double>(i)))
        {
            return makeString(std::to_string(i));
        }
        return jerry_number(static_cast<double>(i));
    }
    case dom::Kind::String:
    case dom::Kind::SafeString:
    {
        auto const& s = v.getString();
        return makeString(s);
    }
    case dom::Kind::Array:
    {
        // Arrays are converted eagerly since they don't have the circular
        // reference problem that objects have (Handlebars options objects
        // contain symbol contexts with parent references, but arrays don't).
        jerry_value_t arr = jerry_array(v.getArray().size());
        uint32_t idx = 0;
        for (auto const& elem: v.getArray())
        {
            jerry_value_t je = toJsValue(elem, impl);
            jerry_value_t sr = jerry_object_set_index(arr, idx++, je);
            jerry_value_free(sr);
            jerry_value_free(je);
        }
        return arr;
    }
    case dom::Kind::Object:
        // Use lazy proxy for objects - properties converted on access.
        // This avoids infinite recursion from circular references in
        // Handlebars options objects (context, data, root contain symbol
        // trees with parent references).
        return makeObjectProxy(v.getObject(), impl);
    case dom::Kind::Function:
        return makeFunctionProxy(v.getFunction(), impl);
    default:
        return jerry_undefined();
    }
}

static dom::Value
toDomValue(jerry_value_t v, std::shared_ptr<Context::Impl> const& impl)
{
    // Convert JerryScript values back into DOM counterparts, wrapping JS
    // functions so native code can call them and translating arrays/objects
    // recursively. Numbers retain integral form when they fit in int64 to match
    // existing template expectations.
    auto lock = lockContext(impl);

    // Check if this is one of our DOM value proxies - if so, return the
    // original dom::Value directly to preserve type information (e.g., arrays
    // remain arrays instead of being converted to objects).
    if (jerry_value_is_proxy(v))
    {
        jerry_value_t handler = jerry_proxy_handler(v);
        if (!jerry_value_is_exception(handler))
        {
            // Native pointer is stored directly on the handler object.
            auto* holder = static_cast<DomValueHolder*>(
                jerry_object_get_native_ptr(handler, &kDomProxyInfo));
            if (holder)
            {
                jerry_value_free(handler);
                return holder->value;
            }
        }
        jerry_value_free(handler);
    }

    if (jerry_value_is_undefined(v) || jerry_value_is_null(v))
    {
        if (jerry_value_is_undefined(v))
        {
            return {dom::Kind::Undefined};
        }
        return {dom::Kind::Null};
    }
    if (jerry_value_is_boolean(v))
    {
        return {(bool) jerry_value_to_boolean(v)};
    }
    if (jerry_value_is_number(v))
    {
        double d = jerry_value_as_number(v);
        if (std::trunc(d) == d
            && d >= (double) std::numeric_limits<std::int64_t>::min()
            && d <= (double) std::numeric_limits<std::int64_t>::max())
        {
            return {static_cast<std::int64_t>(d)};
        }
        return {d};
    }
    if (jerry_value_is_function(v))
    {
        // Wrap the JS function so it can be invoked from DOM helpers.
        // Use weak_ptr to avoid preventing Context cleanup. When the deleter
        // runs, if the Context has been cleaned up (Impl destroyed or
        // cleanup() called), we skip jerry_value_free since JerryScript
        // already released all values during jerry_cleanup().
        //
        // Thread safety tradeoff: We check owner_thread to avoid calling
        // JerryScript from a different thread (which would be undefined
        // behavior). If a dom::Function is destroyed on a different thread,
        // we skip jerry_value_free, causing a temporary JerryScript reference
        // leak until context cleanup. This is preferable to UB.
        auto fnHandle = std::shared_ptr<jerry_value_t>(
            new jerry_value_t(jerry_value_copy(v)),
            [weak_impl = std::weak_ptr<Context::Impl>(impl)](jerry_value_t const* h) {
            if (!h)
            {
                return;
            }
            // Try to lock the weak_ptr. If Impl is still alive, free the value.
            // If Impl is gone or cleanup() was called, the value is already freed.
            if (auto locked = weak_impl.lock())
            {
                if (locked->alive && locked->jerry_ctx && !locked->cleaning_up
                    && locked->owner_thread == std::this_thread::get_id())
                {
                    auto lock = lockContext(locked);
                    jerry_value_free(*h);
                }
            }
            // Always delete the handle memory, even if we skipped jerry_value_free
            delete h;
        });

        return dom::makeVariadicInvocable(
            [fnHandle,
             impl](dom::Array const& args) -> Expected<dom::Value, Error> {
            auto lock = lockContext(impl);
            std::vector<jerry_value_t> jsArgs;
            jsArgs.reserve(args.size());
            for (auto const& a: args)
            {
                jsArgs.push_back(toJsValue(a, impl));
            }

            jerry_value_t ret = jerry_call(
                *fnHandle,
                jerry_undefined(),
                jsArgs.data(),
                jsArgs.size());
            for (auto& a: jsArgs)
            {
                jerry_value_free(a);
            }
            if (jerry_value_is_exception(ret))
            {
                auto err = makeError(ret);
                jerry_value_free(ret);
                return Unexpected(err);
            }
            auto dv = toDomValue(ret, impl);
            jerry_value_free(ret);
            return dv;
        });
    }
    if (jerry_value_is_string(v))
    {
        return {toString(v)};
    }
    if (jerry_value_is_array(v))
    {
        dom::Array arr;
        uint32_t len = jerry_array_length(v);
        for (uint32_t i = 0; i < len; ++i)
        {
            jerry_value_t elem = jerry_object_get_index(v, i);
            if (!jerry_value_is_exception(elem))
            {
                arr.push_back(toDomValue(elem, impl));
            }
            jerry_value_free(elem);
        }
        return {std::move(arr)};
    }
    if (jerry_value_is_object(v))
    {
        dom::Object obj;
        jerry_value_t keys = jerry_object_keys(v);
        uint32_t len = jerry_array_length(keys);
        for (uint32_t i = 0; i < len; ++i)
        {
            jerry_value_t key = jerry_object_get_index(keys, i);
            std::string k = toString(key);
            jerry_value_t val = jerry_object_get(v, key);
            if (!jerry_value_is_exception(val))
            {
                obj.set(k, toDomValue(val, impl));
            }
            jerry_value_free(key);
            jerry_value_free(val);
        }
        jerry_value_free(keys);
        return {std::move(obj)};
    }
    return nullptr;
}

// ------------------------------------------------------------
// registerHelper
// ------------------------------------------------------------

static Expected<Value, Error>
resolveHelperFunction(
    Scope& scope,
    std::string_view name,
    std::string_view script)
{
    // Coerce user-provided helper source into a callable. Resolution order:
    //
    // 1. Parenthesized eval - handles function declarations without side effects
    //    e.g., "function add(a,b) { return a+b; }" -> "(function add(a,b)...)"
    //
    // 2. Direct eval - handles IIFEs and expressions that return functions
    //    e.g., "(function(){ return function(){}; })()"
    //
    // 3. Global lookup - handles scripts that define globals
    //    e.g., "var helper = function(){}; helper;"
    //
    // This order minimizes side effects: parenthesized eval of a function
    // declaration is pure, while direct eval may execute statements.
    Error firstErr("code did not evaluate to a function");

    // Try parenthesized first (common case: function declarations)
    std::string wrapped;
    wrapped.reserve(script.size() + 2);
    wrapped.push_back('(');
    wrapped.append(script);
    wrapped.push_back(')');

    if (auto expr = scope.eval(wrapped))
    {
        if (expr->isFunction())
        {
            return *expr;
        }
    }
    else
    {
        firstErr = expr.error();
    }

    // Try direct eval (IIFEs, expressions)
    if (auto exp = scope.eval(script))
    {
        if (exp->isFunction())
        {
            return *exp;
        }
    }
    else if (firstErr.message() == "code did not evaluate to a function")
    {
        // Keep the more informative error
        firstErr = exp.error();
    }

    // Fall back to global lookup
    if (Value global = scope.getGlobalObject())
    {
        Value candidate = global.get(name);
        if (candidate.isFunction())
        {
            return candidate;
        }
    }

    return Unexpected(
        firstErr.message().empty() ?
            Error(
                std::string("helper is not a function: ") + std::string(name)) :
            firstErr);
}

Expected<void, Error>
registerHelper(
    Handlebars& hbs,
    std::string_view name,
    Context& ctx,
    std::string_view script)
{
    // Bridge a user-supplied helper script into Handlebars: evaluate or
    // resolve the helper into a JS function, expose it on a shared global for
    // reuse, then register a wrapper that handles Handlebars' `options` object
    // with no name-specific shortcuts.
    Scope scope(ctx);
    auto fnExp = resolveHelperFunction(scope, name, script);
    if (!fnExp)
    {
        return Unexpected(fnExp.error());
    }
    Value fn = *fnExp;

    // Store helper on a shared global object so utility scripts can reference
    // registered helpers. Existing helpers are preserved; re-registering a
    // name replaces both the MrDocsHelpers entry and the Handlebars binding.
    Value helpers = scope.getGlobal("MrDocsHelpers").value_or(Value{});
    if (helpers.isUndefined() || !helpers.isObject())
    {
        helpers = scope.pushObject();
        scope.setGlobal("MrDocsHelpers", helpers.getDom());
    }
    helpers.set(name, fn);

    hbs.registerHelper(
        std::string(name),
        dom::makeVariadicInvocable(
            [fn](
                dom::Array const& args) -> Expected<dom::Value, Error> {
        return detail::invokeHelper(fn, args);
    }));

    return {};
}

// ------------------------------------------------------------
// free functions
// ------------------------------------------------------------

std::string
toString(Value const& value)
{
    auto dv = value.getDom();
    if (dv.isString())
    {
        return std::string(dv.getString());
    }
    if (dv.isInteger())
    {
        return std::to_string(dv.getInteger());
    }
    if (dv.isBoolean())
    {
        return dv.getBool() ? "true" : "false";
    }
    return {};
}

bool
operator==(Value const& lhs, Value const& rhs) noexcept
{
    return lhs.getDom() == rhs.getDom();
}

std::strong_ordering
operator<=>(Value const& lhs, Value const& rhs) noexcept
{
    return lhs.getDom() <=> rhs.getDom();
}

Value
operator||(Value const& lhs, Value const& rhs)
{
    return lhs.isTruthy() ? lhs : rhs;
}

Value
operator&&(Value const& lhs, Value const& rhs)
{
    return lhs.isTruthy() ? rhs : lhs;
}

} // namespace mrdocs::js
