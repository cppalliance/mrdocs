// Cross-link symmetric IO helpers from a JavaScript extension.
//
// Every `parse_X` and `format_X` free function gets a `@see` entry
// pointing at its partner. The result is that each function's
// rendered page carries a "See Also" link to the other one, without
// anyone writing `@see` by hand.

function partnerName(name) {
    if (name.indexOf("parse_") === 0) {
        return "format_" + name.slice(6);
    }
    if (name.indexOf("format_") === 0) {
        return "parse_" + name.slice(7);
    }
    return null;
}

function transform_corpus(corpus) {
    for (var i = 0; i < corpus.symbols.length; ++i) {
        var s = corpus.symbols[i];
        if (s.kind !== "function") { continue; }
        var pname = partnerName(s.name);
        if (!pname) { continue; }
        var partner = corpus.lookup(pname);
        if (!partner) { continue; }
        s.doc = {
            sees: [{
                children: [{
                    kind: "reference",
                    literal: pname,
                    id: partner.id
                }]
            }]
        };
    }
}
