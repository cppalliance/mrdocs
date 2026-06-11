// Print the inheritance subtree rooted at a named class.
//
// `corpus.lookup(name)` resolves the entry point once. From there the
// only way down the tree is by id: each record carries a `derived`
// list of base16 ids, and `corpus.get(id)` turns each id back into a
// live symbol proxy. The recursion walks the graph that single-pass
// iteration over `corpus.symbols` cannot reconstruct.

function listSubclasses(corpus, sym, indent) {
    for (var i = 0; i < sym.derived.length; ++i) {
        var child = corpus.get(sym.derived[i]);
        if (child) {
            console.log(indent + child.name);
            listSubclasses(corpus, child, indent + "  ");
        }
    }
}

register_transform(function(corpus) {
    var base = corpus.lookup("Shape");
    if (base) {
        console.log(base.name);
        listSubclasses(corpus, base, "  ");
    }
});
