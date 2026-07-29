//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
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
#include <mrdocs/Engines/JavaScript.hpp>
#include <mrdocs/Handlebars.hpp>
#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
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
#    include <pthread.h>
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
// with JERRY_EXTERNAL_CONTEXT=ON (see utils/bootstrap/patches/jerryscript/
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
dom::Expected<dom::Value>
invokeHelper(Value const& fn, dom::Array const& args)
{
    if (args.empty())
    {
        return Unexpected(dom::Error(
            "handlebars::Handlebars helper called without arguments; "
            "expected options object as last argument"));
    }

    dom::Value const& options = args.back();
    if (!options.isObject())
    {
        return Unexpected(dom::Error(
            "handlebars::Handlebars helper options must be an object; "
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
        return Unexpected(dom::Error(std::string(ret.error().message())));
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

    // Live Context instances (and their copies) sharing this Impl. The
    // DomValueHolder / FunctionHolder objects keep a shared_ptr<Impl>, so the
    // interpreter owns them through a reference cycle; cleanup() breaks that
    // cycle by tearing down the holders. That teardown must run when the last
    // Context goes away rather than be left to ~Impl (which the cycle prevents
    // from ever running), so Context counts its references here.
    std::atomic<int> context_refs{0};

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


// The public symbols are defined in per-symbol impl fragments below;
// this file owns the shared engine machinery and aggregates them.
#include "JavaScript/Context.ipp"
#include "JavaScript/Scope.ipp"
#include "JavaScript/Value.ipp"
#include "JavaScript/ValueBridge.ipp"
#include "JavaScript/registerHelper.ipp"

} // namespace mrdocs::js
