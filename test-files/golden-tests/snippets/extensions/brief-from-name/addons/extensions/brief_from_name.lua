-- Synthesize documentation for every `is_*` predicate from its name.
--
-- The convention: "any function whose name starts with `is_` is a
-- predicate that returns `true` if its name (in plain English) holds".
-- Both the brief and the lone parameter follow that template, so the
-- script writes them once and frees authors from typing the same
-- sentence on every declaration. Anything an author already wrote is
-- preserved: only missing fields are filled in.

register_transform(function(corpus)
    for _, sym in ipairs(corpus.symbols) do
        if sym.kind == "function"
           and sym.name:sub(1, 3) == "is_" then
            if not sym.doc then sym.doc = {} end

            local subject = sym.name:sub(4):gsub("_", " ")

            if not sym.doc.brief then
                sym.doc.brief = "Returns true if " .. subject .. "."
            end

            if #sym.params == 1
               and (not sym.doc.params or #sym.doc.params == 0) then
                sym.doc.params = {
                    {
                        name = sym.params[1].name,
                        children = "The input examined for the "
                            .. subject .. " property."
                    }
                }
            end
        end
    end
end)
