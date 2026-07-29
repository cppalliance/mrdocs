-- Echo helper used in golden tests; keeps output stable across engines.
-- Uses normalize_args from _utils.lua (loaded before helper files).

return function(...)
    local list = normalize_args(...)
    local value
    if list.n > 0 and list[1] ~= nil then
        value = list[1]
    else
        value = ""
    end
    return "lua:" .. tostring(value)
end
