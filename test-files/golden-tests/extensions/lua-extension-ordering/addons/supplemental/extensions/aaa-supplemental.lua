-- Second-running script (lives in the supplemental root, which
-- sorts after the primary root in full-path order). Its rename
-- overwrites the primary root's, so this is the name that must
-- appear in the rendered output.

function transform_corpus(corpus)
    for _, sym in ipairs(corpus.symbols) do
        if sym.kind == "function" then
            mrdocs.set(sym._id, "name", "from_supplemental")
        end
    end
end
