// Synthesize documentation for every `is_*` predicate from its name.
//
// The convention: "any function whose name starts with `is_` is a
// predicate that returns `true` if its name (in plain English) holds".
// Both the brief and the lone parameter follow that template, so the
// script writes them once and frees authors from typing the same
// sentence on every declaration. Anything an author already wrote is
// preserved: only missing fields are filled in.

function transform_corpus(corpus) {
    for (var i = 0; i < corpus.symbols.length; ++i) {
        var sym = corpus.symbols[i];
        if (sym.kind !== "function") { continue; }
        if (sym.name.indexOf("is_") !== 0) { continue; }

        if (!sym.doc) { sym.doc = {}; }

        var subject = sym.name.slice(3).replace(/_/g, " ");

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
}
