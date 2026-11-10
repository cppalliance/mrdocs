mrdocs.register_transform("subclass-tree", function(ctx) {
    const base = ctx.corpus.lookup("Shape");
    if (base) {
        console.log(base.name);
        listSubclasses(ctx.corpus, base, "  ");
    }
});

/**
 * Print the inheritance subtree below a record, one class per line, indented by
 * depth. Each record's `derived` list holds the ids of its direct subclasses;
 * `corpus.get` turns each id back into a symbol, and the recursion follows them
 * down the graph.
 * @param {object} corpus - `ctx.corpus`.
 * @param {object} sym - The record whose subclasses to print.
 * @param {string} indent - The leading whitespace for this level.
 */
function listSubclasses(corpus, sym, indent) {
    for (let i = 0; i < sym.derived.length; ++i) {
        const child = corpus.get(sym.derived[i]);
        if (child) {
            console.log(indent + child.name);
            listSubclasses(corpus, child, indent + "  ");
        }
    }
}
