// Impl fragment of JavaScript.cpp (one TU): the DOM<->JS value bridge
// (proxies + toJsValue/toDomValue; forward-declared near the top of the file).
// Included within `namespace mrdocs::js {`. Not a standalone header.

// ------------------------------------------------------------
// Proxy plumbing shared by the object and array proxies
// ------------------------------------------------------------

// Set `obj[name] = value` and release the temporaries. Ownership of
// `value` stays with the caller.
static void
setNamedProperty(jerry_value_t obj, char const* name, jerry_value_t value)
{
    jerry_value_t key = makeString(name);
    jerry_value_t sr = jerry_object_set(obj, key, value);
    jerry_value_free(sr);
    jerry_value_free(key);
}

// Install `fn` as the `name` trap of a Proxy handler object.
static void
setHandlerTrap(
    jerry_value_t handler,
    char const* name,
    jerry_external_handler_t fn)
{
    jerry_value_t trap = jerry_function_external(fn);
    setNamedProperty(handler, name, trap);
    jerry_value_free(trap);
}

// Build the `{ value, writable, enumerable, configurable }` data
// descriptor a `getOwnPropertyDescriptor` trap returns. Takes ownership
// of `value`.
static jerry_value_t
makeDataDescriptor(
    jerry_value_t value,
    bool writable,
    bool enumerable,
    bool configurable)
{
    jerry_value_t desc = jerry_object();
    setNamedProperty(desc, "value", value);
    jerry_value_free(value);
    jerry_value_t flag = jerry_boolean(writable);
    setNamedProperty(desc, "writable", flag);
    jerry_value_free(flag);
    flag = jerry_boolean(enumerable);
    setNamedProperty(desc, "enumerable", flag);
    jerry_value_free(flag);
    flag = jerry_boolean(configurable);
    setNamedProperty(desc, "configurable", flag);
    jerry_value_free(flag);
    return desc;
}

// Allocate the holder that keeps `value` alive for a proxy and register
// it with the context so cleanup() can reclaim it if the GC never does.
static DomValueHolder*
newDomValueHolder(dom::Value value, std::shared_ptr<Context::Impl> const& impl)
{
    auto* holder = new DomValueHolder();
    holder->impl = impl;
    holder->value = std::move(value);
    impl->registerHolder(holder);
    return holder;
}

// Attach `holder` to `target` and wrap `target` in a Proxy whose handler
// is one of the two shared by the context (see sharedProxyHandler).
// Takes ownership of `target`; the handler stays owned by the context.
// The target's native pointer owns the holder, so `DomValueHolder::
// free_cb` deletes it once the proxy, and with it the target, is
// collected. A proxy therefore costs two engine objects, the target
// and the proxy itself, instead of a handler and a trap function set
// of its own.
static jerry_value_t
finishDomProxy(
    jerry_value_t target,
    jerry_value_t handler,
    DomValueHolder* holder)
{
    jerry_object_set_native_ptr(target, &kDomProxyInfo, holder);
    jerry_value_t proxy = jerry_proxy(target, handler);
    jerry_value_free(target);

    // If proxy creation fails, the handler was still freed above, which
    // triggers free_cb to delete the holder. Return an empty object.
    if (jerry_value_is_exception(proxy))
    {
        jerry_value_free(proxy);
        return jerry_object();
    }
    return proxy;
}

// Parse a canonical array index as defined by ECMAScript: the decimal
// form of an integer in [0, 2^32 - 2] with no sign and no leading zeros.
// Anything else ("length", "map", "01", "-1") is an ordinary property.
static std::optional<std::uint32_t>
parseArrayIndex(std::string_view s)
{
    if (s.empty() || s.size() > 10)
    {
        return std::nullopt;
    }
    if (s.size() > 1 && s[0] == '0')
    {
        return std::nullopt;
    }
    std::uint64_t n = 0;
    for (char c: s)
    {
        if (c < '0' || c > '9')
        {
            return std::nullopt;
        }
        n = n * 10 + static_cast<std::uint64_t>(c - '0');
    }
    if (n >= 0xFFFFFFFFull)
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(n);
}

// ------------------------------------------------------------
// Lazy Object Proxy
// ------------------------------------------------------------
// A JavaScript Proxy that wraps a dom::Object. Properties are converted
// lazily when accessed, avoiding infinite recursion from circular
// references (e.g., symbols that reference parent symbols in Handlebars
// options objects). Every trap finds the wrapped value through the
// native pointer on the proxy target (its first argument), which is
// what lets one handler serve every object proxy of a context.

// Build the handler shared by every object proxy of a context.
static jerry_value_t
makeObjectProxyHandler()
{
    jerry_value_t handler = jerry_object();

    // 'get' trap: handler.get(target, prop, receiver)
    setHandlerTrap(handler, "get",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_undefined();
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_undefined();

            std::string propName = toString(args_p[1]);
            auto lock = lockContext(h->impl);
            dom::Value val = h->value.getObject().get(propName);
            return toJsValue(val, h->impl);
        });

    // 'has' trap: handler.has(target, prop)
    setHandlerTrap(handler, "has",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_boolean(false);
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_boolean(false);

            std::string propName = toString(args_p[1]);
            auto lock = lockContext(h->impl);
            return jerry_boolean(h->value.getObject().exists(propName));
        });

    // 'ownKeys' trap: handler.ownKeys(target)
    setHandlerTrap(handler, "ownKeys",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 1)
                return jerry_array(0);
            auto* h = getHolderFromTarget(args_p[0]);
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

    // 'getOwnPropertyDescriptor' trap (needed for ownKeys to work properly)
    setHandlerTrap(handler, "getOwnPropertyDescriptor",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_undefined();
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_undefined();

            std::string propName = toString(args_p[1]);
            auto lock = lockContext(h->impl);
            if (!h->value.getObject().exists(propName))
                return jerry_undefined();

            return makeDataDescriptor(
                toJsValue(h->value.getObject().get(propName), h->impl),
                /*writable=*/true,
                /*enumerable=*/true,
                /*configurable=*/true);
        });

    // 'set' trap: handler.set(target, prop, value, receiver) -> boolean
    //
    // Delegates the assignment to `dom::Object::set` on the underlying
    // holder. The default `dom::Object` writes to its own overlay; the
    // symbol-proxy implementation used by corpus extensions overrides
    // `set` to mutate the live C++ object instead. A `std::exception`
    // from that override propagates back here and is rethrown as a JS
    // `TypeError` so the script sees a real error instead of a silent
    // assignment.
    setHandlerTrap(handler, "set",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 3)
                return jerry_boolean(false);
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_boolean(false);

            std::string propName = toString(args_p[1]);
            auto lock = lockContext(h->impl);
            dom::Value val = toDomValue(args_p[2], h->impl);

            try
            {
                h->value.getObject().set(propName, val);
            }
            catch (std::exception const& ex)
            {
                return jerry_throw_sz(JERRY_ERROR_TYPE, ex.what());
            }
            return jerry_boolean(true);
        });

    return handler;
}

// ------------------------------------------------------------
// Lazy Array Proxy
// ------------------------------------------------------------
// Creates a JavaScript Proxy that wraps a dom::Array. Elements are
// converted one at a time when a script reads them, so exposing a large
// array (e.g. `ctx.corpus.symbols`, one entry per symbol in the corpus)
// costs nothing until an element is visited, and a loop over it keeps
// only the current element alive on the JerryScript heap. Converting the
// array eagerly instead used to build one object proxy per element up
// front, which on a corpus of a few thousand symbols exhausted the
// fixed-size heap and left the engine thrashing in the collector.
//
// The target is a real (empty) `Array` carrying the holder as its native
// pointer, so `Array.isArray` and
// `JSON.stringify` treat the proxy as an array, and every property that
// is not an index or `length` (`map`, `forEach`, `join`, `push`,
// `Symbol.iterator`, ...) is forwarded to the target and resolves through
// `Array.prototype`. Those methods are generic: called with the proxy as
// `this`, they read `length` and the elements through the traps below.
//
// Reads see the live `dom::Array`, and index writes go through
// `dom::Array::set`, so a script mutation is visible from C++ and vice
// versa. A read-only implementation (the `dom::ArrayImpl` default, used
// by `ctx.corpus.symbols`) throws from `set`, which surfaces as a JS
// `TypeError`, so `sort` or an index assignment on such an array fails
// loudly instead of silently doing nothing.
//
// The array cannot be resized from a script, because `dom::Array` has
// no way to shrink. A write to `length` is accepted only when it equals
// the current size, which is what `Array.prototype.push` writes after
// storing the new element; any other value throws a `TypeError`, and so
// does deleting an element. `pop`, `shift` and `splice` therefore throw
// instead of leaving the array with a stale length or duplicated tail.
// Scripts that need to trim or reorder copy the array with `slice()`.

// Build the handler shared by every array proxy of a context.
static jerry_value_t
makeArrayProxyHandler()
{
    jerry_value_t handler = jerry_object();

    // 'get' trap: handler.get(target, prop, receiver)
    setHandlerTrap(handler, "get",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_undefined();
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_undefined();
            jerry_value_t const target = args_p[0];
            jerry_value_t const prop = args_p[1];
            if (!jerry_value_is_string(prop))
                return jerry_object_get(target, prop);

            std::string propName = toString(prop);
            auto lock = lockContext(h->impl);
            dom::Array const arr = h->value.getArray();
            if (propName == "length")
                return jerry_number(static_cast<double>(arr.size()));
            if (auto idx = parseArrayIndex(propName))
            {
                if (*idx >= arr.size())
                    return jerry_undefined();
                return toJsValue(arr.get(*idx), h->impl);
            }
            return jerry_object_get(target, prop);
        });

    // 'has' trap: handler.has(target, prop)
    setHandlerTrap(handler, "has",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_boolean(false);
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_boolean(false);
            jerry_value_t const target = args_p[0];
            jerry_value_t const prop = args_p[1];
            if (!jerry_value_is_string(prop))
                return jerry_object_has(target, prop);

            std::string propName = toString(prop);
            auto lock = lockContext(h->impl);
            if (propName == "length")
                return jerry_boolean(true);
            if (auto idx = parseArrayIndex(propName))
                return jerry_boolean(*idx < h->value.getArray().size());
            return jerry_object_has(target, prop);
        });

    // 'ownKeys' trap: handler.ownKeys(target)
    //
    // Reports every index plus `length`. The target's own `length` is
    // non-configurable, and a Proxy must list all such keys.
    setHandlerTrap(handler, "ownKeys",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 1)
                return jerry_array(0);
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_array(0);

            auto lock = lockContext(h->impl);
            auto const n = static_cast<uint32_t>(h->value.getArray().size());
            jerry_value_t keys = jerry_array(n + 1);
            for (uint32_t i = 0; i < n; ++i)
            {
                jerry_value_t key = makeString(std::to_string(i));
                jerry_value_t setRes = jerry_object_set_index(keys, i, key);
                jerry_value_free(setRes);
                jerry_value_free(key);
            }
            jerry_value_t lengthKey = makeString("length");
            jerry_value_t setRes = jerry_object_set_index(keys, n, lengthKey);
            jerry_value_free(setRes);
            jerry_value_free(lengthKey);
            return keys;
        });

    // 'getOwnPropertyDescriptor' trap: handler.getOwnPropertyDescriptor(target, prop)
    //
    // `length` mirrors a real array's descriptor (writable, non-enumerable,
    // non-configurable); reporting it configurable would violate the Proxy
    // invariant against the target's own non-configurable `length`.
    setHandlerTrap(handler, "getOwnPropertyDescriptor",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_undefined();
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_undefined();
            if (!jerry_value_is_string(args_p[1]))
                return jerry_undefined();

            std::string propName = toString(args_p[1]);
            auto lock = lockContext(h->impl);
            dom::Array const arr = h->value.getArray();
            if (propName == "length")
            {
                return makeDataDescriptor(
                    jerry_number(static_cast<double>(arr.size())),
                    /*writable=*/true,
                    /*enumerable=*/false,
                    /*configurable=*/false);
            }
            auto idx = parseArrayIndex(propName);
            if (!idx || *idx >= arr.size())
                return jerry_undefined();
            return makeDataDescriptor(
                toJsValue(arr.get(*idx), h->impl),
                /*writable=*/true,
                /*enumerable=*/true,
                /*configurable=*/true);
        });

    // 'set' trap: handler.set(target, prop, value, receiver) -> boolean
    //
    // Index writes go to `dom::Array::set`, and a `std::exception` from
    // the implementation (read-only array) becomes a JS `TypeError`.
    // `length` is accepted only when unchanged (see above). Anything
    // else lands on the target as an ordinary expando property.
    setHandlerTrap(handler, "set",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 3)
                return jerry_boolean(false);
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_boolean(false);
            jerry_value_t const target = args_p[0];
            jerry_value_t const prop = args_p[1];
            jerry_value_t const value = args_p[2];

            std::optional<std::uint32_t> idx;
            std::string propName;
            if (jerry_value_is_string(prop))
            {
                propName = toString(prop);
                if (propName == "length")
                {
                    auto lock = lockContext(h->impl);
                    auto const size = static_cast<double>(
                        h->value.getArray().size());
                    if (jerry_value_is_number(value)
                        && jerry_value_as_number(value) == size)
                        return jerry_boolean(true);
                    return jerry_throw_sz(JERRY_ERROR_TYPE,
                        "cannot resize a DOM array from a script; "
                        "copy it with slice() first");
                }
                idx = parseArrayIndex(propName);
            }
            if (!idx)
            {
                jerry_value_t r = jerry_object_set(target, prop, value);
                bool const ok = !jerry_value_is_exception(r)
                    && jerry_value_to_boolean(r);
                jerry_value_free(r);
                return jerry_boolean(ok);
            }

            auto lock = lockContext(h->impl);
            dom::Value val = toDomValue(value, h->impl);
            try
            {
                h->value.getArray().set(*idx, std::move(val));
            }
            catch (std::exception const& ex)
            {
                return jerry_throw_sz(JERRY_ERROR_TYPE, ex.what());
            }
            return jerry_boolean(true);
        });

    // 'deleteProperty' trap: handler.deleteProperty(target, prop) -> boolean
    //
    // Elements and `length` cannot be removed (see above). Other keys are
    // expandos on the target and delete normally.
    setHandlerTrap(handler, "deleteProperty",
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 2)
                return jerry_boolean(false);
            auto* h = getHolderFromTarget(args_p[0]);
            if (!h)
                return jerry_boolean(false);
            jerry_value_t const target = args_p[0];
            jerry_value_t const prop = args_p[1];
            if (jerry_value_is_string(prop))
            {
                std::string const propName = toString(prop);
                if (propName == "length" || parseArrayIndex(propName))
                    return jerry_throw_sz(JERRY_ERROR_TYPE,
                        "cannot delete elements of a DOM array; "
                        "copy it with slice() first");
            }
            return jerry_object_delete(target, prop);
        });

    return handler;
}

// The two handlers of a context, built on first use and released by
// Context::Impl::cleanup(). Traps read the wrapped value from the proxy
// target, so nothing in a handler is specific to one proxy.
static jerry_value_t
sharedProxyHandler(std::shared_ptr<Context::Impl> const& impl, bool forArray)
{
    if (!impl->haveProxyHandlers)
    {
        impl->objectProxyHandler = makeObjectProxyHandler();
        impl->arrayProxyHandler = makeArrayProxyHandler();
        impl->haveProxyHandlers = true;
    }
    return forArray ? impl->arrayProxyHandler : impl->objectProxyHandler;
}

static jerry_value_t
makeObjectProxy(dom::Object obj, std::shared_ptr<Context::Impl> impl)
{
    auto* holder = newDomValueHolder(dom::Value(std::move(obj)), impl);
    // An empty target: the proxy intercepts all access
    jerry_value_t target = jerry_object();
    return finishDomProxy(target, sharedProxyHandler(impl, false), holder);
}

static jerry_value_t
makeArrayProxy(dom::Array arr, std::shared_ptr<Context::Impl> impl)
{
    auto* holder = newDomValueHolder(dom::Value(std::move(arr)), impl);
    // A real, empty Array as target (see above)
    jerry_value_t target = jerry_array(0);
    return finishDomProxy(target, sharedProxyHandler(impl, true), holder);
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
        // Use lazy proxy for arrays - elements converted on access, so a
        // loop over a large array (every symbol in a corpus) only ever
        // holds the current element on the JerryScript heap.
        return makeArrayProxy(v.getArray(), impl);
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
        jerry_value_t target = jerry_proxy_target(v);
        if (!jerry_value_is_exception(target))
        {
            // The native pointer lives on the proxy target.
            auto* holder = getHolderFromTarget(target);
            if (holder)
            {
                jerry_value_free(target);
                return holder->value;
            }
        }
        jerry_value_free(target);
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
             impl](dom::Array const& args) -> dom::Expected<dom::Value> {
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
                return Unexpected(dom::Error(std::string(err.message())));
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
