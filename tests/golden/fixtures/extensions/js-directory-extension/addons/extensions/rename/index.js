// The entry of a JavaScript extension in a directory of its own. JerryScript
// has no module loader, so the directory holds this one file.

mrdocs.register_transform("rename", function(ctx)
{
    for (var i = 0; i < ctx.corpus.symbols.length; ++i)
    {
        var sym = ctx.corpus.symbols[i];
        if (sym.kind === "function")
        {
            sym.name = sym.name + "_from_index";
        }
    }
});
