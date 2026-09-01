// Check each symbol's documentation against the project's conventions. Every
// convention is a small predicate; this transform warns about each symbol that
// breaks one, routing through mrdocs.report so the messages count as real
// diagnostics. The one convention checked here flags a "low-quality brief": a
// brief that reads like a stray comment rather than a description.
mrdocs.register_transform("project-conventions", function(ctx) {
    for (let i = 0; i < ctx.corpus.symbols.length; ++i) {
        const sym = ctx.corpus.symbols[i];
        const brief = symbolBrief(sym);
        if (isLowQualityBrief(brief)) {
            mrdocs.report.warn(sym.name + ': low-quality brief: "' + brief + '"');
        }
    }
});

/**
 * The text of a documentation node, gathered by walking its children.
 * @param {object} node - A documentation node with `literal` and `children`.
 * @returns {string} The concatenated text of the node and its descendants.
 */
function nodeText(node) {
    let text = node.literal || "";
    if (node.children) {
        for (let i = 0; i < node.children.length; ++i) {
            text += nodeText(node.children[i]);
        }
    }
    return text;
}

/**
 * A symbol's brief as plain text, trimmed, or "" when it has none.
 * @param {object} sym - The symbol to read.
 * @returns {string} The brief text, or an empty string.
 */
function symbolBrief(sym) {
    if (!sym.doc || !sym.doc.brief) {
        return "";
    }
    return nodeText(sym.doc.brief).replace(/^\s+|\s+$/g, "");
}

/**
 * Whether a brief is low-quality: it reads like a stray comment rather than a
 * description because it is too short or more punctuation than words.
 * @param {string} brief - The brief text, already trimmed.
 * @returns {boolean} True when the brief breaks the convention.
 */
function isLowQualityBrief(brief) {
    if (brief.length === 0) {
        return false;
    }
    let special = 0, total = 0;
    for (let i = 0; i < brief.length; ++i) {
        if (brief[i] === " ") {
            continue;
        }
        total += 1;
        if (!/[A-Za-z0-9]/.test(brief[i])) {
            special += 1;
        }
    }
    return brief.length < 4 || special > total / 2;
}
