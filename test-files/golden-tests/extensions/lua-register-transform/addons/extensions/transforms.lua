-- Declare two corpus transforms with `register_transform`. Both run, in
-- registration order, so one extension can contribute several
-- transforms. The first renames every function; the second rewrites
-- its brief.

register_transform(function(corpus)
    for _, sym in ipairs(corpus.symbols) do
        if sym.kind == "function" then
            sym.name = "renamed_" .. sym.name
        end
    end
end)

register_transform(function(corpus)
    for _, sym in ipairs(corpus.symbols) do
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
