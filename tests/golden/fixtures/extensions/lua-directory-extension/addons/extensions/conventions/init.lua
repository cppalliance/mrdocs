-- The entry of a Lua extension in a directory of its own: Lua's convention
-- for a directory module. The directory is searched first by `require`,
-- so `require "rules"` finds rules.lua next to this file.

local rules = require("rules")

mrdocs.register_transform("conventions", function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" then
            sym.name = rules.rename(sym.name)
        end
    end
end)
