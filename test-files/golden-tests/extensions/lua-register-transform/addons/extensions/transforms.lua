-- Declare two corpus transforms with `mrdocs.register_transform`. Both run, in
-- registration order, so one extension can contribute several
-- transforms. The first renames every function; the second rewrites
-- its brief.

mrdocs.register_transform(function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.name = "renamed_" .. sym.name
        end
    end
end)

mrdocs.register_transform(function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.doc = {
                brief = {
                    children = {
                        { kind = "text",
                          literal = "Brief from the second transform" }
                    }
                }
            }
        end
    end
end)
