-- A transform that reads its own parameters from `ctx.params` (the
-- `transform-options.<id>` block keyed by the id it registered under)
-- and writes them into the corpus. Here it sets every function's brief
-- from `ctx.params.text`.

mrdocs.register_transform("brief-from-params", function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.doc = {
                brief = {
                    children = {
                        { kind = "text", literal = ctx.params.text }
                    }
                }
            }
        end
    end
end)
