-- Describe helper: reports type and value in a deterministic string.
-- Uses normalize_args and format_object from _utils.lua.

return function(...)
    local list = normalize_args(...)
    local typ
    local value

    if list.n == 0 then
        typ = "undefined"
        value = ""
    elseif list.n > 1 then
        typ = "array"
        local strs = {}
        for i = 1, list.n do
            strs[i] = tostring(list[i])
        end
        value = table.concat(strs, ",")
    else
        local v = list[1]
        if v == nil then
            typ = "null"
            value = ""
        else
            typ = type(v)
            if typ == "table" then
                value = format_object(v)
            else
                value = tostring(v)
            end
        end
    end

    return typ .. ":" .. value
end
