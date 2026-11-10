-- Helpers shared by the self-documentation checks.

local helpers = {}

-- Namespaces that mirror a library with its own naming and are not part of
-- the plugin API, one reason each.
helpers.EXEMPT_NAMESPACES = {
    ["mrdocs::polyfill::"] = "polyfills named after the std:: types they replace",
    ["mrdocs::describe::"] = "reflection helpers named after Boost.Describe",
}

--- The corpus symbols keyed by id.
--- @param ctx table The transform context.
--- @return table Symbols keyed by id.
function helpers.index_by_id(ctx)
    local by_id = {}
    for _, sym in ipairs(ctx.corpus.symbols) do
        by_id[sym.id] = sym
    end
    return by_id
end

--- The fully qualified name of a symbol, built by walking its parents.
--- @param by_id table Symbols keyed by id.
--- @param sym table The symbol to name.
--- @return string The name joined with `::`, without the global namespace.
function helpers.qualified_name(by_id, sym)
    local parts = { sym.name }
    local parent = by_id[sym.parent]
    while parent and parent.name and parent.name ~= "" do
        table.insert(parts, 1, parent.name)
        parent = by_id[parent.parent]
    end
    return table.concat(parts, "::")
end

--- Whether a symbol is an implementation detail: it, or one of its parents,
--- is a private or protected member, or sits in a namespace named `detail`.
--- @param by_id table Symbols keyed by id.
--- @param sym table The symbol to check.
--- @return boolean True when the symbol is not part of the public interface.
function helpers.is_implementation_detail(by_id, sym)
    local current = sym
    while current and current.name and current.name ~= "" do
        if current.access == "private" or current.access == "protected" then
            return true
        end
        if current.kind == "namespace" and current.name == "detail" then
            return true
        end
        current = by_id[current.parent]
    end
    return false
end

--- Whether a qualified name sits in a namespace that mirrors another library.
--- @param qualified string The qualified name of the symbol.
--- @return boolean True when the namespace is exempt.
function helpers.is_exempt_namespace(qualified)
    for prefix in pairs(helpers.EXEMPT_NAMESPACES) do
        if qualified:sub(1, #prefix) == prefix then
            return true
        end
    end
    return false
end

-- Member type names every standard container declares, so a nested type
-- with one of these names stands for the std:: member type.
helpers.CONTAINER_MEMBER_TYPES = {
    iterator = true,
    const_iterator = true,
    reverse_iterator = true,
    const_reverse_iterator = true,
    value_type = true,
    size_type = true,
    difference_type = true,
    reference = true,
    const_reference = true,
    pointer = true,
    const_pointer = true,
    key_type = true,
    mapped_type = true,
}

--- The corpus symbols grouped by the id of their parent.
--- @param ctx table The transform context.
--- @return table Lists of symbols keyed by parent id.
function helpers.index_by_parent(ctx)
    local children = {}
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.parent then
            children[sym.parent] = children[sym.parent] or {}
            table.insert(children[sym.parent], sym)
        end
    end
    return children
end

-- Standard traits a project trait derives from to get its `type` member.
local STD_TRAIT_BASES = {
    conditional = true, enable_if = true, type_identity = true,
    integral_constant = true, bool_constant = true,
    true_type = true, false_type = true,
    remove_const = true, remove_volatile = true, remove_cv = true,
    remove_reference = true, remove_cvref = true, remove_pointer = true,
    remove_extent = true, remove_all_extents = true,
    add_const = true, add_volatile = true, add_cv = true,
    add_lvalue_reference = true, add_rvalue_reference = true,
    add_pointer = true,
}

--- Whether a record is a trait: it has a public member type named `type`,
--- its name ends in `_traits`, it derives from a standard trait named in
--- `STD_TRAIT_BASES` (`std::conditional`, `std::add_const`, ...), whether or
--- not the corpus holds that base, or it derives from a record in the corpus
--- that qualifies, like `add_cv_from` derives from `add_const_from`.
--- @param by_id table Symbols keyed by id.
--- @param children table Symbols grouped by parent id.
--- @param sym table The symbol to check.
--- @return boolean True when the symbol is a trait record.
function helpers.is_trait(by_id, children, sym)
    local seen = {}
    local function check(record)
        if seen[record.id] then
            return false
        end
        seen[record.id] = true
        if record.kind ~= "record" then
            return false
        end
        if record.name:match("_traits$") then
            return true
        end
        for _, member in ipairs(children[record.id] or {}) do
            if member.name == "type"
                and member.access == "public"
                and not member.inheritedFrom
                and (member.kind == "typedef" or member.kind == "record") then
                return true
            end
        end
        for _, base in ipairs(record.bases or {}) do
            local name = base.type and base.type.name
            local prefix = name and name.prefix
            if name and STD_TRAIT_BASES[name.identifier]
                and prefix and prefix.identifier == "std" then
                return true
            end
            local base_record = name and name.id and by_id[name.id]
            if base_record and check(base_record) then
                return true
            end
        end
        return false
    end
    return check(sym)
end

--- Whether a symbol is a type nested in a record under a name the standard
--- containers use for their member types, like `iterator` or `value_type`.
--- @param by_id table Symbols keyed by id.
--- @param sym table The symbol to check.
--- @return boolean True when the symbol is a container member type.
function helpers.is_container_member_type(by_id, sym)
    local parent = by_id[sym.parent]
    return parent ~= nil
        and parent.kind == "record"
        and helpers.CONTAINER_MEMBER_TYPES[sym.name] == true
end

return helpers
