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
// - Context: Owns an isolated JerryScript interpreter with its own heap
//   (see "JerryScript heap" below). Multiple Contexts can exist
//   simultaneously; the count is not
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
// - DOM → JS (toJsValue): Objects and arrays use lazy Proxy wrappers, so
//   properties and elements are converted only when a script reads them.
//   This avoids infinite recursion from circular references (e.g.,
//   Handlebars symbol contexts) and keeps a loop over a large array (every
//   symbol in a corpus) from materializing it on the small JerryScript
//   heap. Functions wrap dom::Function.
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
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <jerryscript.h>
#include <jerryscript-port.h>
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
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
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
// The context port functions and jerry_port_fatal are excluded from
// jerry-port when building with JERRY_EXTERNAL_CONTEXT=ON (see
// utils/bootstrap/patches/jerryscript/CMakeLists.txt), so mrdocs provides
// the only implementations. All other port functions (jerry_port_log,
// jerry_port_sleep, etc.) use the default implementations from jerry-port.

// ------------------------------------------------------------
// Thread-Local Storage for JerryScript Context
// ------------------------------------------------------------
//
// With JERRY_EXTERNAL_CONTEXT every access the engine makes to its own
// state goes through jerry_port_context_get(): each JERRY_CONTEXT(field)
// read is a call to it. The engine makes that call several times per
// bytecode instruction, so it has to be as cheap as a memory load. A
// constant-initialized thread_local pointer is exactly that: no dynamic
// initializer, no destructor, so the compiler emits no TLS wrapper
// function and no atexit registration, and the access compiles to one
// thread-pointer-relative load on every platform, including GCC static
// executables. Anything heavier here (a key lookup behind a once-guard,
// for instance) slows every JavaScript workload by about 3x.
//
// The engine itself does not even call jerry_port_context_get: the
// JerryScript build force-includes utils/bootstrap/patches/jerryscript/
// mrdocs-context.h, which defines that call as a macro reading
// `jerry_port_context_tls`, so an engine field access is one TLS load with
// no call at all. The port function stays for the API and returns the same
// pointer, so an engine built without the header behaves the same, slower.
#if defined(_MSC_VER)
#define MRDOCS_JERRY_THREAD_LOCAL __declspec(thread)
#else
#define MRDOCS_JERRY_THREAD_LOCAL __thread
#endif
extern "C" {
extern MRDOCS_JERRY_THREAD_LOCAL jerry_context_t* jerry_port_context_tls;
}
// The definition takes C linkage from the declaration above. GCC rejects
// an initializer on a declaration spelled `extern "C" ... = nullptr`.
MRDOCS_JERRY_THREAD_LOCAL jerry_context_t* jerry_port_context_tls = nullptr;
static void* get_tls_jerry_context() { return jerry_port_context_tls; }
static void set_tls_jerry_context(void* ptr)
{
    jerry_port_context_tls = static_cast<jerry_context_t*>(ptr);
}

// ------------------------------------------------------------
// JerryScript heap
// ------------------------------------------------------------
//
// JerryScript manages its own arena: one contiguous block handed to it by
// jerry_port_context_alloc at jerry_init, addressed through 32-bit
// compressed pointers (offsets from the block start in 8-byte units). The
// engine never asks the OS for more, so the block's size is the hard limit
// on what a script can keep alive, and because the engine stores the size
// in a uint32_t the ceiling is 4 GB.
//
// Reserving the block is not the same as consuming it. On POSIX the block
// comes from mmap with MAP_NORESERVE: the pages are demand-zero and cost
// physical memory only when the engine first writes to them, and heap
// initialization writes a single free-list header. So every context
// reserves the maximum the engine can address and pays only for what its
// scripts allocate. Collection frequency does not depend on the block
// size either: the engine collects every JERRY_GC_LIMIT bytes of net
// growth, 8 KB by default, and that default is the right one here. The
// allocator keeps one address-ordered free list and inserts each freed
// block by walking it, so a large step (an 8 MB step was tried) frees
// thousands of blocks per collection into a long list and makes every
// symbol loop several times slower. Windows has no
// demand-zero pages of that kind: memory must be committed before it can
// be written, and a commit is charged in full against the system-wide
// commit limit at VirtualAlloc time. So on Windows the block is only
// reserved (address space, no commit), and a vectored exception handler
// commits it 1 MB at a time when the engine first touches a page (see
// jerryCommitOnDemand). The engine gives its host no other notice before
// it writes, so this is the one way to get the same lazy behavior there
// without an upstream change. On any platform a reservation can still
// fail in a restricted environment (a process address-space limit such
// as ulimit -v, Linux strict overcommit which ignores MAP_NORESERVE); the
// size is then halved until it succeeds, down to the 512 KB the engine
// used before compressed pointers were widened.
//
// When a script does exhaust the block, JerryScript retries garbage
// collection under rising pressure and then calls jerry_port_fatal, which
// cannot return. There is no JavaScript exception for that case and no
// allocation hook, so the best the host can do is say what happened (see
// jerry_port_fatal below).

// Just under 4 GB, the most the engine's uint32_t heap size can express
// once the context structure is subtracted.
static constexpr std::size_t kMaxJerryHeapSize =
    (std::size_t(4) << 30) - (std::size_t(1) << 20);
static constexpr std::size_t kMinJerryHeapSize = std::size_t(512) << 10;

// The heap size to try first for a new context: the maximum, since it is
// reserved lazily on every platform and costs virtual address space only.
// A failed reservation is halved from here (see jerry_port_context_alloc).
static std::size_t
defaultJerryHeapSize()
{
    return kMaxJerryHeapSize;
}

// Reserve `size` bytes of zeroed, page-aligned address space, or nullptr.
// Nothing is committed up front: POSIX pages are demand-zero, and on
// Windows jerryCommitOnDemand commits pages as they are first touched.
static void*
reserveJerryBlock(std::size_t size)
{
#if defined(_WIN32)
    return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
#else
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(MAP_NORESERVE)
    flags |= MAP_NORESERVE;
#endif
    void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
#endif
}

static void
releaseJerryBlock(void* p, std::size_t size)
{
#if defined(_WIN32)
    (void) size;
    VirtualFree(p, 0, MEM_RELEASE);
#else
    munmap(p, size);
#endif
}

// Size of every live block, keyed by its address. jerry_port_context_free
// takes no arguments and jerry_port_fatal wants to report the size of the
// heap that ran out, so both look it up here.
static std::mutex&
jerryBlocksMutex()
{
    static std::mutex m;
    return m;
}

static std::unordered_map<void*, std::size_t>&
jerryBlocks()
{
    static std::unordered_map<void*, std::size_t> blocks;
    return blocks;
}

static std::size_t
jerryBlockSize(void* p)
{
    std::lock_guard<std::mutex> lock(jerryBlocksMutex());
    auto it = jerryBlocks().find(p);
    return it == jerryBlocks().end() ? 0 : it->second;
}

#if defined(_WIN32)
// How much to commit per first-touch fault. Larger chunks mean fewer
// faults (4 GB / 1 MB = 4096 at most); smaller ones waste less on a
// script that barely uses its heap.
static constexpr std::size_t kJerryCommitChunk = std::size_t(1) << 20;

// The reserved block that contains `addr`, or {nullptr, 0}.
static std::pair<void*, std::size_t>
jerryBlockContaining(void const* addr)
{
    auto const a = reinterpret_cast<std::uintptr_t>(addr);
    std::lock_guard<std::mutex> lock(jerryBlocksMutex());
    for (auto const& [base, size]: jerryBlocks())
    {
        auto const b = reinterpret_cast<std::uintptr_t>(base);
        if (a >= b && a < b + size)
            return {base, size};
    }
    return {nullptr, 0};
}

// Vectored exception handler that turns the first touch of a reserved
// but uncommitted page of a JerryScript block into a commit of the 1 MB
// chunk around it, then resumes the faulting instruction. Faults outside
// our blocks, and every other exception, pass through untouched. Runs on
// the faulting thread with normal stack and locks, so taking the registry
// mutex here is fine: nothing holds it while touching block memory.
// Under a debugger each first touch shows up as a first-chance access
// violation before the handler runs; that is expected.
static LONG WINAPI
jerryCommitOnDemand(PEXCEPTION_POINTERS info)
{
    auto const* rec = info->ExceptionRecord;
    if (rec->ExceptionCode != EXCEPTION_ACCESS_VIOLATION
        || rec->NumberParameters < 2)
        return EXCEPTION_CONTINUE_SEARCH;
    auto const fault = static_cast<std::uintptr_t>(rec->ExceptionInformation[1]);
    auto const [base, size] = jerryBlockContaining(
        reinterpret_cast<void const*>(fault));
    if (!base)
        return EXCEPTION_CONTINUE_SEARCH;
    auto const begin = reinterpret_cast<std::uintptr_t>(base);
    auto const chunkStart = fault - ((fault - begin) % kJerryCommitChunk);
    auto const chunkEnd = std::min(chunkStart + kJerryCommitChunk, begin + size);
    if (!VirtualAlloc(reinterpret_cast<void*>(chunkStart),
            chunkEnd - chunkStart, MEM_COMMIT, PAGE_READWRITE))
    {
        // The system commit limit is exhausted. There is no way to hand
        // this back to the engine as an allocation failure, so stop with
        // a message rather than let the access violation propagate.
        mrdocs::report::error(
            "JavaScript engine out of memory: Windows could not commit more "
            "memory for a script's heap (system commit limit reached). "
            "Keep fewer symbol objects alive at once, or move the work to Lua.");
        std::fflush(nullptr);
        std::_Exit(static_cast<int>(JERRY_FATAL_OUT_OF_MEMORY));
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void
ensureCommitOnDemandHandler()
{
    static std::once_flag once;
    std::call_once(once, []{ AddVectoredExceptionHandler(1, jerryCommitOnDemand); });
}
#endif

// Reserves the block for a new JerryScript context: the context structure
// followed by the heap. Called by jerry_init(), which reads the returned
// total size to compute the heap size and then finds the block through
// jerry_port_context_get(), so the pointer is published in TLS here.
// Context::Impl captures it and restores the previous TLS value afterward.
extern "C" size_t
jerry_port_context_alloc(size_t context_size)
{
#if defined(_WIN32)
    ensureCommitOnDemandHandler();
#endif
    std::size_t heap = defaultJerryHeapSize();
    for (;;)
    {
        std::size_t const total = context_size + heap;
        if (void* p = reserveJerryBlock(total))
        {
            {
                std::lock_guard<std::mutex> lock(jerryBlocksMutex());
                jerryBlocks()[p] = total;
            }
            set_tls_jerry_context(p);
            return total;
        }
        if (heap <= kMinJerryHeapSize)
        {
            break;
        }
        heap /= 2;
    }
    // jerry_init() would dereference a null context next, so stop here
    // with a message rather than crash. Not even 512 KB could be reserved.
    mrdocs::report::error(
        "JavaScript engine: cannot reserve memory for a new context");
    std::exit(EXIT_FAILURE);
}

// Frees the block of the current context. Called internally by
// jerry_cleanup(), which passes no arguments; the block is the one in TLS.
extern "C" void
jerry_port_context_free(void)
{
    void* ctx = get_tls_jerry_context();
    if (!ctx) // LCOV_EXCL_LINE
        return; // LCOV_EXCL_LINE
    std::size_t size = 0;
    {
        std::lock_guard<std::mutex> lock(jerryBlocksMutex());
        auto it = jerryBlocks().find(ctx);
        if (it != jerryBlocks().end())
        {
            size = it->second;
            jerryBlocks().erase(it);
        }
    }
    if (size != 0)
    {
        releaseJerryBlock(ctx, size);
    }
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

// Terminal engine failure. JerryScript calls this after garbage collection
// could not free enough heap (JERRY_FATAL_OUT_OF_MEMORY) or on an internal
// error, and it must not return: the engine state is inconsistent and
// unwinding a C++ exception through the engine's C frames is undefined.
// The upstream default exits silently with the code, which is how a
// script that outgrew its heap used to look like a hang or a crash with no
// message. Report what happened and how big the heap was, then exit.
extern "C" void
jerry_port_fatal(jerry_fatal_code_t code)
{
    if (code == JERRY_FATAL_OUT_OF_MEMORY)
    {
        std::size_t const bytes = jerryBlockSize(get_tls_jerry_context());
        mrdocs::report::error(
            "JavaScript engine out of memory: a script exhausted its "
            "{} MB heap, the most the engine can address. Keep fewer "
            "symbol objects alive at once, or move the work to Lua, "
            "which has no such limit.",
            bytes >> 20);
    }
    else
    {
        mrdocs::report::error("JavaScript engine fatal error (code {})",
            static_cast<int>(code));
    }
    // Leave through _Exit: the engine state is gone, and running static
    // destructors (which tear down live Contexts) would call back into it.
    std::fflush(nullptr);
    std::_Exit(code == JERRY_FATAL_OUT_OF_MEMORY ? static_cast<int>(code) : EXIT_FAILURE);
}

#if !defined(_WIN32)
// The non-Windows jerry_port_init lives in the same upstream file as
// jerry_port_fatal and is excluded with it. Nothing to initialize.
extern "C" void
jerry_port_init(void)
{
}
#endif

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
        return Unexpected(dom::Error(std::string(ret.error().reason())));
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

    // Proxy handler objects shared by every DOM proxy of this context, one
    // for objects and one for arrays (see ValueBridge.ipp). Built on first
    // use; released in cleanup() before the engine is torn down.
    jerry_value_t objectProxyHandler = 0;
    jerry_value_t arrayProxyHandler = 0;
    bool haveProxyHandlers = false;

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
        // jerry_init() calls jerry_port_context_alloc(), which reserves the
        // block and publishes it through TLS so jerry_port_context_get()
        // finds it during initialization. Capture that pointer, then
        // restore whatever context this thread had active before.
        void* prev_ctx = get_tls_jerry_context();
        jerry_init(JERRY_INIT_EMPTY);
        jerry_ctx = get_tls_jerry_context();
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

        if (haveProxyHandlers)
        {
            jerry_value_free(objectProxyHandler);
            jerry_value_free(arrayProxyHandler);
            haveProxyHandlers = false;
        }

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

// Retrieve the DomValueHolder from a DOM proxy's target object, which
// carries it as a native pointer. Returns nullptr for any other object.
static DomValueHolder*
getHolderFromTarget(jerry_value_t target)
{
    return static_cast<DomValueHolder*>(
        jerry_object_get_native_ptr(target, &kDomProxyInfo));
}

// The public symbols are defined in per-symbol impl fragments below;
// this file owns the shared engine machinery and aggregates them.
#include "JavaScript/Context.ipp"
#include "JavaScript/Scope.ipp"
#include "JavaScript/Value.ipp"
#include "JavaScript/ValueBridge.ipp"
#include "JavaScript/registerHelper.ipp"

} // namespace mrdocs::js
