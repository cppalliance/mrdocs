-- Glue helper: joins positional args using the first argument as separator.
-- Uses normalize_args from _utils.lua.

return function(...)
    local list = normalize_args(...)
    if list.n == 0 then
        return ""
    end
    local sep = tostring(list[1])
    local items = {}
    for i = 2, list.n do
        items[#items + 1] = tostring(list[i])
    end
    return table.concat(items, sep)
end
