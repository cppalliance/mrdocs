mrdocs.register_transform("count-by-kind", function(ctx) {
    var counts = {};
    for (var i = 0; i < ctx.corpus.symbols.length; ++i) {
        var k = ctx.corpus.symbols[i].kind;
        counts[k] = (counts[k] || 0) + 1;
    }
    for (var k in counts) {
        console.log(k + ": " + counts[k]);
    }
});
