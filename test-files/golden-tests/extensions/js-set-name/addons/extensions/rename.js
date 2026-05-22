// Mutate every function from a JavaScript extension. Exercises two
// shapes of the generic setter: a scalar string assignment (`name`)
// and a polymorphic-aware nested object whose leaves are
// `Polymorphic<Inline>` values selected by a kebab-case `kind` tag.
//
// Mirrors the lua-set-name fixture but exercises the JS path: the
// `mrdocs` global is exposed as a JavaScript object whose `set`
// entry is a native function backed by a dom::Function in C++.

function transform_corpus(corpus)
{
    for (var i = 0; i < corpus.symbols.length; ++i)
    {
        var sym = corpus.symbols[i];
        if (sym.kind === "function")
        {
            mrdocs.set(sym._id, "name", "renamed_" + sym.name);
            mrdocs.set(sym._id, "doc", {
                brief: {
                    children: [
                        { kind: "text",
                          literal: "Brief rewritten by JS extension" }
                    ]
                }
            });
        }
    }
}
