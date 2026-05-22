-- Replace a function's return type from a Lua extension.
--
-- The motivating use case is rendering coroutine-returning functions
-- in terms of a concept rather than the underlying coroutine type.
-- For example, a library might want every function returning
-- `capy::task<T>` to advertise its return type as the `Awaitable`
-- concept (cross-linked to the concept's documentation page):
--
--   local awaitable_id = nil
--   for _, s in ipairs(corpus.symbols) do
--       if s.kind == "concept" and s.name == "Awaitable" then
--           awaitable_id = s._id
--           break
--       end
--   end
--   ...
--   mrdocs.set(fn._id, "returnType", {
--       kind = "named",
--       name = {
--           kind = "identifier",
--           identifier = "Awaitable",
--           id = awaitable_id  -- cross-links the type to the concept
--       }
--   })
--
-- This fixture omits the lookup and uses a bare identifier so the
-- test is self-contained.

function transform_corpus(corpus)
    for _, sym in ipairs(corpus.symbols) do
        if sym.kind == "function" and sym.name == "target_function" then
            mrdocs.set(sym._id, "returnType", {
                kind = "named",
                name = {
                    kind = "identifier",
                    identifier = "Awaitable"
                }
            })
        end
    end
end
