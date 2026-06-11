// Mutate every function from a JavaScript extension. Exercises two
// shapes of direct assignment: a scalar string write (`name`) and a
// nested-object write whose leaves are `Polymorphic<Inline>` values
// selected by a kebab-case `kind` tag.
//
// Mirrors the lua-set-name fixture but exercises the JS path: the
// proxy's `set` trap forwards each assignment into the live C++
// Symbol via reflection.

register_transform(function(corpus)
{
    for (var i = 0; i < corpus.symbols.length; ++i)
    {
        var sym = corpus.symbols[i];
        if (sym.kind === "function")
        {
            sym.name = "renamed_" + sym.name;
            sym.doc = {
                brief: {
                    children: [
                        { kind: "text",
                          literal: "Brief rewritten by JS extension" }
                    ]
                }
            };
        }
    }
});
