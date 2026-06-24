-- Exercise the documented "Pass `nil` to clear an optional field"
-- behavior: setting `doc` to `nil` from Lua must clear the symbol's
-- doc-comment so the rendered output contains no doc-comment block
-- for it.

mrdocs.register_transform("clear-doc", function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.doc = nil
        end
    end
end)
