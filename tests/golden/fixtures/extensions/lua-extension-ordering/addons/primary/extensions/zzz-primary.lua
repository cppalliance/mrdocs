-- Second-running script. The primary root comes before the supplemental one
-- in the configuration, but extensions load in the order of their file names,
-- and "zzz-primary.lua" sorts after "aaa-supplemental.lua". Its rename
-- overwrites the other script's, so this is the name that must appear in the
-- rendered output.

mrdocs.register_transform("zzz-primary", function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.name = "from_primary"
        end
    end
end)
