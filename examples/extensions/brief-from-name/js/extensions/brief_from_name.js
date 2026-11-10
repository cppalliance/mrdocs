mrdocs.register_transform("brief-from-name", function(ctx) {
    for (let i = 0; i < ctx.corpus.symbols.length; ++i) {
        fillPredicateDoc(ctx.corpus.symbols[i]);
    }
});

/**
 * Fill in the brief and parameter doc of an `is_*` predicate from its name.
 * The subject is the name after `is_` with underscores turned to spaces, so
 * `is_prime` becomes "Returns true if prime." Does nothing for a symbol that is
 * not an `is_*` function, and never overwrites a field an author already wrote.
 * @param {object} sym - The symbol to document, mutated in place.
 */
function fillPredicateDoc(sym) {
    if (sym.kind !== "function" || sym.name.indexOf("is_") !== 0) {
        return;
    }
    if (!sym.doc) { sym.doc = {}; }

    const subject = sym.name.slice(3).replace(/_/g, " ");

    if (!sym.doc.brief) {
        sym.doc.brief = "Returns true if " + subject + ".";
    }

    if (sym.params.length === 1
        && (!sym.doc.params || sym.doc.params.length === 0)) {
        sym.doc.params = [{
            name: sym.params[0].name,
            children: "The input examined for the " + subject + " property."
        }];
    }
}
