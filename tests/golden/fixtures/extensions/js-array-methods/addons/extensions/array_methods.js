// Drive `ctx.corpus.symbols` through Array.prototype instead of an index
// loop. The corpus array is a lazy proxy over a live C++ container, and
// these methods are generic over `length` and indexed reads, so `filter`,
// `forEach`, `map` and `for...of` must all see every symbol and the
// `set` trap must still reach the live Symbol when called on an element
// produced by them.

mrdocs.register_transform("array-methods", function(ctx)
{
    var symbols = ctx.corpus.symbols;
    if (!Array.isArray(symbols))
        throw new Error("ctx.corpus.symbols is not an array");

    var functions = symbols.filter(function(sym) {
        return sym.kind === "function";
    });

    // The order of ctx.corpus.symbols is not stable across platforms, so
    // nothing here may depend on an element's position: the new name is
    // built from the function's own name and the count of functions.
    var names = functions.map(function(sym) { return sym.name; });
    var total = 0;
    for (var sym of symbols)
        ++total;
    if (total < functions.length || names.length !== functions.length)
        throw new Error("iteration and map disagree on the symbol count");

    functions.forEach(function(sym) {
        sym.name = "renamed_" + sym.name + "_of_" + functions.length + "_functions";
    });
});
