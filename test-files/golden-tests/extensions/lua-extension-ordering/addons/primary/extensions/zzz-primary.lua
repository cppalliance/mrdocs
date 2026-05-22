-- Even though this file's basename ("zzz-primary.lua") sorts AFTER
-- the supplemental root's ("aaa-supplemental.lua"), the docs promise
-- that scripts run in alphabetical order by FULL PATH. The primary
-- root sorts before the supplemental root ("primary" < "supplemental"),
-- so this script runs FIRST; its rename is overwritten by the
-- supplemental's.

function transform_corpus(corpus)
    for _, sym in ipairs(corpus.symbols) do
        if sym.kind == "function" then
            mrdocs.set(sym._id, "name", "from_primary")
        end
    end
end
