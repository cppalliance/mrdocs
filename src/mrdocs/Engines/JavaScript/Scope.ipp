// Impl fragment of JavaScript.cpp (one TU): the js::Scope methods.
// Included within `namespace mrdocs::js {`. Not a standalone header.

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
