-- First-running script. Its file name ("aaa-supplemental.lua") sorts before
-- the other root's ("zzz-primary.lua"), and extensions load in the order of
-- their names whatever root they are in, so this one runs first, although its
-- root is the later one. Its rename is overwritten by the other script's.

mrdocs.register_transform("aaa-supplemental", function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.name = "from_supplemental"
        end
    end
end)
