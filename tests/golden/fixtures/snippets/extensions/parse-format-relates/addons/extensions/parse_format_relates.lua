local partnerName

mrdocs.register_transform("parse-format-relates", function(ctx)
    for _, s in ipairs(ctx.corpus.symbols) do
        if s.kind == "function" then
            local pname = partnerName(s.name)
            if pname then
                local partner = ctx.corpus.lookup(pname)
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

--- The name of a function's symmetric IO partner: `format_X` for a `parse_X`,
--- and `parse_X` for a `format_X`. Returns nil for any name that is neither.
--- @param name string The function's name.
--- @return string|nil partner The partner's name, or nil if `name` is not a
---   `parse_`/`format_` helper.
function partnerName(name)
    local partner = nil
    if name:sub(1, 6) == "parse_" then
        partner = "format_" .. name:sub(7)
    elseif name:sub(1, 7) == "format_" then
        partner = "parse_" .. name:sub(8)
    end
    return partner
end
