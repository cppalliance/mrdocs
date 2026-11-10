mrdocs.register_transform("count-by-kind", function(ctx)
    local counts = {}
    for _, sym in ipairs(ctx.corpus.symbols) do
        counts[sym.kind] = (counts[sym.kind] or 0) + 1
    end
    for k, v in pairs(counts) do
        print(k .. ": " .. v)
    end
end)
