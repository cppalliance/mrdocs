-- Script engine benchmark, Lua side. The same six workloads as bench.js,
-- in the same order, timed with os.clock from inside the script.
mrdocs.register_transform("bench-lua", function(ctx)
    local s = ctx.corpus.symbols
    local t = os.clock(); local counts = {}
    for _, sym in ipairs(s) do local k = sym.kind; counts[k] = (counts[k] or 0) + 1 end
    print(string.format("bench-lua: count-by-kind %.0f ms", (os.clock() - t) * 1000))
    t = os.clock(); local names = {}
    for _, sym in ipairs(s) do if sym.kind == "function" then names[#names + 1] = sym.name end end
    local r = #table.concat(names, ",")
    print(string.format("bench-lua: collect-function-names %.0f ms (%d)", (os.clock() - t) * 1000, #names))
    t = os.clock(); local parts = {}
    for _, sym in ipairs(s) do parts[#parts + 1] = sym.kind .. " " .. tostring(sym.name) .. "\n" end
    local out = table.concat(parts)
    print(string.format("bench-lua: build-text %.0f ms (%d chars)", (os.clock() - t) * 1000, #out))
    t = os.clock(); local deep = 0
    for _, sym in ipairs(s) do local loc = sym.loc; if loc and loc.def and loc.def.location and loc.def.location.lineNumber then deep = deep + loc.def.location.lineNumber end end
    print(string.format("bench-lua: nested-reads %.0f ms (%d)", (os.clock() - t) * 1000, deep))
    t = os.clock(); local acc = 0
    for i = 0, 4999999 do acc = acc + i % 7 end
    print(string.format("bench-lua: numeric-loop %.0f ms (%d)", (os.clock() - t) * 1000, acc))
    t = os.clock(); local kept = {}
    for _, sym in ipairs(s) do kept[#kept + 1] = sym end
    print(string.format("bench-lua: keep-all-proxies %.0f ms (%d)", (os.clock() - t) * 1000, #kept))
end)
