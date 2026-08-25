local fillPredicateDoc

mrdocs.register_transform("brief-from-name", function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        fillPredicateDoc(sym)
    end
end)

--- Fill in the brief and parameter doc of an `is_*` predicate from its name.
--- The subject is the name after `is_` with underscores turned to spaces, so
--- `is_prime` becomes "Returns true if prime." Does nothing for a symbol that
--- is not an `is_*` function, and never overwrites a field an author already
--- wrote.
--- @param sym table The symbol to document, mutated in place.
function fillPredicateDoc(sym)
    if sym.kind ~= "function" or sym.name:sub(1, 3) ~= "is_" then
        return
    end
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
