// Impl fragment of JavaScript.cpp (one TU): the DOM<->JS value bridge
// (proxies + toJsValue/toDomValue; forward-declared near the top of the file).
// Included within `namespace mrdocs::js {`. Not a standalone header.

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

    // 'set' trap: handler.set(target, prop, value, receiver) -> boolean
    //
    // Delegates the assignment to `dom::Object::set` on the underlying
    // holder. The default `dom::Object` writes to its own overlay; the
    // symbol-proxy implementation used by corpus extensions overrides
    // `set` to mutate the live C++ object instead. A `std::exception`
    // from that override propagates back here and is rethrown as a JS
    // `TypeError` so the script sees a real error instead of a silent
    // assignment.
    jerry_value_t set_fn = jerry_function_external(
        [](jerry_call_info_t const* call_info_p,
           jerry_value_t const args_p[],
           jerry_length_t argc) -> jerry_value_t
        {
            if (argc < 3)
                return jerry_boolean(false);
            auto* h = getHolderFromHandler(call_info_p->this_value);
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

    jerry_value_t set_key = makeString("set");
    sr = jerry_object_set(handler, set_key, set_fn);
    jerry_value_free(sr);
    jerry_value_free(set_key);
    jerry_value_free(set_fn);

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
