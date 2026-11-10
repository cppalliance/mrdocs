local helpers = require("helpers")

-- Member functions of the std:: containers, strings, string views and
-- optional types. A class that mirrors one of them (see `mirrors_std_type`)
-- keeps these snake_case names whatever style its own members use.
local STD_CONTAINER_MEMBERS = {
    assign = true, at = true, back = true, begin = true, capacity = true,
    cbegin = true, cend = true, clear = true, contains = true, count = true,
    crbegin = true, crend = true, data = true, emplace = true,
    emplace_back = true, emplace_front = true, empty = true,
    ["end"] = true, equal_range = true, erase = true, find = true,
    front = true, insert = true, lower_bound = true, max_size = true,
    pop_back = true, pop_front = true, push_back = true, push_front = true,
    rbegin = true, rend = true, reserve = true, resize = true,
    shrink_to_fit = true, size = true, swap = true, upper_bound = true,
    c_str = true, has_value = true, value_or = true, remove_prefix = true,
    remove_suffix = true, starts_with = true, ends_with = true,
    find_first_of = true, find_last_of = true, find_first_not_of = true,
    find_last_not_of = true, push_range = true, append_range = true,
}

-- Free functions named like the std:: function they stand for.
local STD_FREE_FUNCTIONS = {
    to_underlying = true,
    find_last_of = true,
}

local classify, is_free_exempt, is_checked_function, mirrors_std_type

-- Guard the naming convention for functions in the `mrdocs` namespace, which
-- follows LLVM: camelCase.
--
-- * Free functions use camelCase. The LLVM casts (`isa`, `cast`, `dyn_cast`
--   and their `_or_null` forms), std:: look-alikes (`make_` factories and
--   the names in `STD_FREE_FUNCTIONS`), `tag_invoke` and the generated
--   `mrdocs_*_descriptor_fn` hooks keep their names.
-- * The member functions of one class use snake_case throughout or camelCase
--   throughout; a single lowercase word such as `render` fits both, and
--   PascalCase is a style of its own that never passes. Special members,
--   operators and, in a class that mirrors a std:: type, the names in
--   `STD_CONTAINER_MEMBERS` do not count.
--
-- Functions in a `detail` namespace, private and protected members, inherited
-- members and the `polyfill` and `describe` namespaces are skipped. Each
-- violation is reported through ctx.report, so the self-documentation test
-- (which runs with warnings as errors) fails on it.
mrdocs.register_transform("function-names", function(ctx)
    local by_id = helpers.index_by_id(ctx)
    local children = helpers.index_by_parent(ctx)
    local styles = {}
    local reported = {}
    for _, sym in ipairs(ctx.corpus.symbols) do
        if is_checked_function(sym) then
            local parent = by_id[sym.parent]
            local qualified = helpers.qualified_name(by_id, sym)
            if qualified:sub(1, 8) == "mrdocs::"
                and not helpers.is_exempt_namespace(qualified)
                and not helpers.is_implementation_detail(by_id, sym) then
                if parent and parent.kind == "namespace" then
                    local style = classify(sym.name)
                    if style ~= "camel"
                        and style ~= "word"
                        and not is_free_exempt(sym.name)
                        and not reported[qualified] then
                        reported[qualified] = true
                        ctx.report.warn(qualified
                            .. ": free functions should use camelCase (the LLVM convention)")
                    end
                elseif parent and parent.kind == "record"
                    and not (STD_CONTAINER_MEMBERS[sym.name]
                        and mirrors_std_type(children, parent)) then
                    local entry = styles[parent.id]
                        or { snake = 0, camel = 0, pascal = 0 }
                    styles[parent.id] = entry
                    local style = classify(sym.name)
                    if entry[style] then
                        entry[style] = entry[style] + 1
                    end
                end
            end
        end
    end
    for id, entry in pairs(styles) do
        local used = (entry.snake > 0 and 1 or 0) + (entry.camel > 0 and 1 or 0)
            + (entry.pascal > 0 and 1 or 0)
        if used > 1 or entry.pascal > 0 then
            local record = helpers.qualified_name(by_id, by_id[id])
            if not reported[record] then
                reported[record] = true
                ctx.report.warn(record
                    .. ": member functions of a class should all use snake_case"
                    .. " or all use camelCase (LLVM uses camelCase)")
            end
        end
    end
end)

--- Whether a function is subject to the naming rule: an ordinary function
--- declared in the corpus itself, not a constructor, destructor, conversion
--- function, operator or inherited member.
--- @param sym table The symbol to check.
--- @return boolean True when the function name is checked.
function is_checked_function(sym)
    return sym.kind == "function"
        and sym.extraction == "regular"
        and not sym.inheritedFrom
        and sym.funcClass == "normal"
        and sym.overloadedOperator == nil
        and sym.name ~= nil
        and sym.name:match("^[%a_]") ~= nil
        and sym.name:sub(1, 8) ~= "operator"
end

--- Whether a record mirrors a std:: container, string or optional: it
--- declares a member type under one of the names those types use, such as
--- `iterator` or `value_type`, or it is iterable (has `begin` and `end`).
--- @param children table Symbols grouped by parent id.
--- @param record table The record to check.
--- @return boolean True when the record has a std-style member type.
function mirrors_std_type(children, record)
    local has_begin, has_end = false, false
    for _, member in ipairs(children[record.id] or {}) do
        if helpers.CONTAINER_MEMBER_TYPES[member.name] then
            return true
        end
        has_begin = has_begin or member.name == "begin"
        has_end = has_end or member.name == "end"
    end
    return has_begin and has_end
end

--- The style of a function name: `word` for one lowercase word, which fits
--- both styles, `snake` for `format_duration`, `camel` for `formatDuration`,
--- `pascal` for `FormatDuration` and `other` for anything else.
--- @param name string The unqualified function name.
--- @return string One of `word`, `snake`, `camel`, `pascal` or `other`.
function classify(name)
    if name:match("^%l[%l%d]*$") then
        return "word"
    elseif name:match("^%l[%l%d]*_[%l%d_]*$")
        and not name:match("__") and not name:match("_$") then
        return "snake"
    elseif name:match("^%l[%w]*$") then
        return "camel"
    elseif name:match("^%u[%w]*$") then
        return "pascal"
    end
    return "other"
end

--- Whether a free function keeps a name outside camelCase: the LLVM casts,
--- std:: look-alikes, `tag_invoke` and the generated descriptor hooks.
--- @param name string The unqualified function name.
--- @return boolean True when the name is excused.
function is_free_exempt(name)
    local base = name:gsub("_or_null$", "")
    return base == "isa"
        or base == "cast"
        or base == "dyn_cast"
        or STD_FREE_FUNCTIONS[name] == true
        or name:match("^make_") ~= nil
        or name == "tag_invoke"
        or name:match("^mrdocs_.*_descriptor_fn$") ~= nil
end
