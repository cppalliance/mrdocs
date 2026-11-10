-- A module of the `conventions` extension, loaded by its entry file.

local rules = {}

function rules.rename(name)
    return name .. "_by_module"
end

return rules
