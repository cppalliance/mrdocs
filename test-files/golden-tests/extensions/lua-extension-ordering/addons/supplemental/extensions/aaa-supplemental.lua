-- Second-running script (lives in the supplemental root, which
-- sorts after the primary root in full-path order). Its rename
-- overwrites the primary root's, so this is the name that must
-- appear in the rendered output.

mrdocs.register_transform(function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.name = "from_supplemental"
        end
    end
end)
