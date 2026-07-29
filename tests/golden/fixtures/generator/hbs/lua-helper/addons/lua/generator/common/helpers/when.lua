-- Block helper that exercises options.fn/options.inverse. Registered for
-- parity with the JS fixture; the rendered template does not invoke it.

return function(condition, options)
    if options == nil or type(options) ~= "table" then
        return ""
    end
    if condition then
        return ""
    end
    return ""
end
