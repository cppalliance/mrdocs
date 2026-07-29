// Impl fragment of JavaScript.cpp (one TU): the js::Value methods and free functions.
// Included within `namespace mrdocs::js {`. Not a standalone header.

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
