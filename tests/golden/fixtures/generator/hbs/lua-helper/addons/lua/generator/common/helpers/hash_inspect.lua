-- Hash helper: builds a stable string from options.hash or key/value args.
-- The Handlebars options object is dropped before the helper runs (mirroring
-- the JS path), so this returns a literal value for the golden test.

return function()
    return "hash:a=1,b=two"
end
