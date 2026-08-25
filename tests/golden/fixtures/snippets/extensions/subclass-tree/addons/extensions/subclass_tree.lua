local listSubclasses

mrdocs.register_transform("subclass-tree", function(ctx)
    local base = ctx.corpus.lookup("Shape")
    if base then
        print(base.name)
        listSubclasses(ctx.corpus, base, "  ")
    end
end)

--- Print the inheritance subtree below a record, one class per line, indented
--- by depth. Each record's `derived` list holds the ids of its direct
--- subclasses; `corpus.get` turns each id back into a symbol, and the recursion
--- follows them down the graph.
--- @param corpus table `ctx.corpus`.
--- @param sym table The record whose subclasses to print.
--- @param indent string The leading whitespace for this level.
function listSubclasses(corpus, sym, indent)
    for _, id in ipairs(sym.derived) do
        local child = corpus.get(id)
        if child then
            print(indent .. child.name)
            listSubclasses(corpus, child, indent .. "  ")
        end
    end
end
