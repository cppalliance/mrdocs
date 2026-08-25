local url_for

mrdocs.register_generator("search-index", function(ctx)
  local entries = {}
  for _, sym in ipairs(ctx.corpus.symbols) do
    local name = sym.name or ""
    if name ~= "" then
      local entry = {}
      entry.name = name
      entry.url = url_for(ctx.corpus, sym)
      entries[#entries + 1] = entry
    end
  end
  ctx.output.write("search-index.json", ctx.stringify(entries))
end)

--- A symbol's page URL: the chain of anchors from the global namespace down to
--- the symbol, joined by "/", plus ".html". Walk up through `parent` (a symbol
--- id) with `corpus.get`; the global namespace has no parent, so it contributes
--- no path segment.
--- @param corpus table `ctx.corpus`, used to resolve each `parent` id.
--- @param sym table The symbol to build a URL for.
--- @return string url The symbol's page URL.
function url_for(corpus, sym)
  local parts = {}
  local cur = sym
  while cur and cur.parent do
    table.insert(parts, 1, cur.anchor)
    cur = corpus.get(cur.parent)
  end
  return table.concat(parts, "/") .. ".html"
end
