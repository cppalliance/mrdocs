// Cross-link symmetric IO helpers from a JavaScript extension.
//
// Every `parse_X` and `format_X` free function gets a `@see` entry
// pointing at its partner. The result is that each function's
// rendered page carries a "See Also" link to the other one, without
// anyone writing `@see` by hand.

function partnerName(name) {
    var partner = null;
    if (name.indexOf("parse_") === 0) {
        partner = "format_" + name.slice(6);
    } else if (name.indexOf("format_") === 0) {
        partner = "parse_" + name.slice(7);
    }
    return partner;
}

mrdocs.register_transform(function(ctx) {
    for (var i = 0; i < ctx.corpus.symbols.length; ++i) {
        var s = ctx.corpus.symbols[i];
        if (s.kind === "function") {
            var pname = partnerName(s.name);
            var partner = pname ? ctx.corpus.lookup(pname) : null;
            if (partner) {
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
    }
});
