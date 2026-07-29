// Impl fragment of JavaScript.cpp (one TU): js::registerHelper and its helper-resolution support.
// Included within `namespace mrdocs::js {`. Not a standalone header.

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
    handlebars::Handlebars& hbs,
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
                dom::Array const& args) -> dom::Expected<dom::Value> {
        return detail::invokeHelper(fn, args);
    }));

    return {};
}
