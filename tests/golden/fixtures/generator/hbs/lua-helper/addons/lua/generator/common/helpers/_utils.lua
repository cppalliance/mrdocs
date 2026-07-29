-- Shared utility functions for Lua helpers.
-- Files starting with '_' are loaded before helper files and define
-- globals that can be used by all helpers.

-- Normalize Handlebars arguments. Returns a packed table whose `n` field
-- is the argument count (so nils round-trip correctly). Userdata (DOM
-- objects passed in by Handlebars when no positional args are given) is
-- filtered out, mirroring the JS `normalize_args` helper.
function normalize_args(...)
    local list = table.pack(...)

    local filtered = { n = 0 }
    for i = 1, list.n do
        local v = list[i]
        if type(v) ~= "userdata" then
            filtered.n = filtered.n + 1
            filtered[filtered.n] = v
        end
    end
    return filtered
end

-- Format an object's key-value pairs as a sorted, comma-separated string.
function format_object(obj)
    local keys = {}
    for k in pairs(obj) do
        keys[#keys + 1] = k
    end
    table.sort(keys)
    local parts = {}
    for _, key in ipairs(keys) do
        parts[#parts + 1] = key .. "=" .. tostring(obj[key])
    end
    return table.concat(parts, ",")
end
