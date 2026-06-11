-- Cross-link symmetric IO helpers from a Lua extension.
--
-- Every `parse_X` and `format_X` free function gets a `@see` entry
-- pointing at its partner. The result is that each function's
-- rendered page carries a "See Also" link to the other one, without
-- anyone writing `@see` by hand.

local function partnerName(name)
    local partner = nil
    if name:sub(1, 6) == "parse_" then
        partner = "format_" .. name:sub(7)
    elseif name:sub(1, 7) == "format_" then
        partner = "parse_" .. name:sub(8)
    end
    return partner
end

register_transform(function(corpus)
    for _, s in ipairs(corpus.symbols) do
        if s.kind == "function" then
            local pname = partnerName(s.name)
            if pname then
                local partner = corpus.lookup(pname)
                if partner then
                    s.doc = {
                        sees = {
                            {
                                children = {
                                    { kind = "reference",
                                      literal = pname,
                                      id = partner.id }
                                }
                            }
                        }
                    }
                end
            end
        end
    end
end)
