-- Exercise the documented "Pass `nil` to clear an optional field"
-- behavior: setting `doc` to `nil` from Lua must clear the symbol's
-- doc-comment so the rendered output contains no doc-comment block
-- for it.

register_transform(function(corpus)
    for _, sym in ipairs(corpus.symbols) do
        if sym.kind == "function" then
            sym.doc = nil
        end
    end
end)
