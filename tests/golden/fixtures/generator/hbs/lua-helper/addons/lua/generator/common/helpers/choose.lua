-- Block helper exercising options.fn/options.inverse. The options table is
-- dropped before the helper runs (matching the JS path), so this always
-- returns the "otherwise" branch.

return function(options)
    if options == nil or type(options) ~= "table" then
        return "otherwise"
    end
    return "otherwise"
end
