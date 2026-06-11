// Declare two corpus transforms with `register_transform`. Both run, in
// registration order, so one extension can contribute several
// transforms. The first renames every function; the second rewrites
// its brief. Mirrors the lua-register-transform fixture on the JS path.

register_transform(function(corpus)
{
    for (var i = 0; i < corpus.symbols.length; ++i)
    {
        var sym = corpus.symbols[i];
        if (sym.kind === "function")
        {
            sym.name = "renamed_" + sym.name;
        }
    }
});

register_transform(function(corpus)
{
    for (var i = 0; i < corpus.symbols.length; ++i)
    {
        var sym = corpus.symbols[i];
        if (sym.kind === "function")
        {
            sym.doc = {
                brief: {
                    children: [
                        { kind: "text",
                          literal: "Brief from the second transform" }
                    ]
                }
            };
        }
    }
});
