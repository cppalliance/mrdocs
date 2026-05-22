-- Mutate every function from a Lua extension. Exercises two shapes
-- of the generic setter: a scalar string assignment (`name`) and a
-- polymorphic-aware nested object whose leaves are
-- `Polymorphic<Inline>` values selected by a kebab-case `kind` tag.
--
-- corpus.symbols is a regular Lua sequence: 1-indexed, with `#` and
-- `ipairs`/`pairs` support.

function transform_corpus(corpus)
    for _, sym in ipairs(corpus.symbols) do
        if sym.kind == "function" then
            mrdocs.set(sym._id, "name", "renamed_" .. sym.name)
            mrdocs.set(sym._id, "doc", {
                brief = {
                    children = {
                        { kind = "text",
                          literal = "Brief rewritten by Lua extension" }
                    }
                }
            })
        end
    end
end
