mrdocs.register_transform("parse-format-relates", function(ctx) {
    for (let i = 0; i < ctx.corpus.symbols.length; ++i) {
        const s = ctx.corpus.symbols[i];
        if (s.kind === "function") {
            const pname = partnerName(s.name);
            const partner = pname ? ctx.corpus.lookup(pname) : null;
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

/**
 * The name of a function's symmetric IO partner: `format_X` for a `parse_X`,
 * and `parse_X` for a `format_X`. Returns null for any name that is neither.
 * @param {string} name - The function's name.
 * @returns {string|null} The partner's name, or null if `name` is not a
 *   `parse_`/`format_` helper.
 */
function partnerName(name) {
    let partner = null;
    if (name.indexOf("parse_") === 0) {
        partner = "format_" + name.slice(6);
    } else if (name.indexOf("format_") === 0) {
        partner = "parse_" + name.slice(7);
    }
    return partner;
}
