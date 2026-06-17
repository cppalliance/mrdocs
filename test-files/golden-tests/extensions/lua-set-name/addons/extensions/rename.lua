-- Mutate every function from a Lua extension. Exercises two shapes
-- of direct assignment: a scalar string write (`name`) and a
-- nested-object write whose leaves are `Polymorphic<Inline>` values
-- selected by a kebab-case `kind` tag.
--
-- ctx.corpus.symbols is a regular Lua sequence: 1-indexed, with `#` and
-- `ipairs`/`pairs` support.

mrdocs.register_transform(function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.name = "renamed_" .. sym.name
            sym.doc = {
                brief = {
                    children = {
                        { kind = "text",
                          literal = "Brief rewritten by Lua extension" }
                    }
                }
            }
        end
    end
end)
