-- Print the inheritance subtree rooted at a named class.
--
-- `ctx.corpus.lookup(name)` resolves the entry point once. From there the
-- only way down the tree is by id: each record carries a `derived`
-- list of base16 ids, and `ctx.corpus.get(id)` turns each id back into a
-- live symbol proxy. The recursion walks the graph that single-pass
-- iteration over `ctx.corpus.symbols` cannot reconstruct.

local function listSubclasses(corpus, sym, indent)
    for _, id in ipairs(sym.derived) do
        local child = corpus.get(id)
        if child then
            print(indent .. child.name)
            listSubclasses(corpus, child, indent .. "  ")
        end
    end
end

mrdocs.register_transform("subclass-tree", function(ctx)
    local base = ctx.corpus.lookup("Shape")
    if base then
        print(base.name)
        listSubclasses(ctx.corpus, base, "  ")
    end
end)
