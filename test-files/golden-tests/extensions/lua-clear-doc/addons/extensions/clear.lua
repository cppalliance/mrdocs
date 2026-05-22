-- Exercise the documented "Pass `null` to clear an optional field"
-- behavior: setting `doc` to `nil` from Lua must clear the symbol's
-- doc-comment so the rendered output contains no doc-comment block
-- for it.

function transform_corpus(corpus)
    for _, sym in ipairs(corpus.symbols) do
        if sym.kind == "function" then
            mrdocs.set(sym._id, "doc", nil)
        end
    end
end
