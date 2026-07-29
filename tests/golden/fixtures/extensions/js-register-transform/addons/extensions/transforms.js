// Declare two corpus transforms with `mrdocs.register_transform`. Both run, in
// registration order, so one extension can contribute several
// transforms. The first renames every function; the second rewrites
// its brief. Mirrors the lua-register-transform fixture on the JS path.

mrdocs.register_transform("rename-functions", function(ctx)
{
    for (var i = 0; i < ctx.corpus.symbols.length; ++i)
    {
        var sym = ctx.corpus.symbols[i];
        if (sym.kind === "function")
        {
            sym.name = "renamed_" + sym.name;
        }
    }
});

mrdocs.register_transform("set-brief", function(ctx)
{
    for (var i = 0; i < ctx.corpus.symbols.length; ++i)
    {
        var sym = ctx.corpus.symbols[i];
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
