-- Print the inheritance subtree rooted at a named class.
--
-- `corpus.lookup(name)` resolves the entry point once. From there the
-- only way down the tree is by id: each record carries a `derived`
-- list of base16 ids, and `corpus.get(id)` turns each id back into a
-- live symbol proxy. The recursion walks the graph that single-pass
-- iteration over `corpus.symbols` cannot reconstruct.

local function listSubclasses(corpus, sym, indent)
    for _, id in ipairs(sym.derived) do
        local child = corpus.get(id)
        if child then
            print(indent .. child.name)
            listSubclasses(corpus, child, indent .. "  ")
        end
    end
end

function transform_corpus(corpus)
    local base = corpus.lookup("Shape")
    if not base then return end
    print(base.name)
    listSubclasses(corpus, base, "  ")
end
