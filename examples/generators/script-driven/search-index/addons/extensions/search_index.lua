-- Declare a `search-index` generator: a script-defined generator that
-- aggregates every symbol into a single search-index.json, the kind of
-- artifact the per-page generators cannot produce.
--
-- `mrdocs.register_generator(id, fn)` declares it next to any
-- `mrdocs.register_transform` a script might also declare; selecting
-- `generator: <id>` runs `fn` with one `ctx`. `ctx.corpus.symbols` is every
-- symbol, each carrying the same fields the templates see (here `name`,
-- `anchor`, and `parent`). `ctx.corpus.get` resolves a `parent` id to its
-- symbol, `ctx.stringify` serializes a value to JSON, and `ctx.output.write`
-- emits files under the output directory.

mrdocs.register_generator("search-index", function(ctx)
  -- A symbol's page URL is the chain of anchors from the global namespace
  -- down to the symbol, joined by "/", plus ".html". Walk up through
  -- `parent` (a symbol id) with `ctx.corpus.get`; the global namespace has no
  -- parent, so it contributes no path segment.
  local function url_for(sym)
    local parts = {}
    local cur = sym
    while cur and cur.parent do
      table.insert(parts, 1, cur.anchor)
      cur = ctx.corpus.get(cur.parent)
    end
    return table.concat(parts, "/") .. ".html"
  end

  local entries = {}
  for _, sym in ipairs(ctx.corpus.symbols) do
    local name = sym.name or ""
    if name ~= "" then
      entries[#entries + 1] = { name = name, url = url_for(sym) }
    end
  end
  ctx.output.write("search-index.json", ctx.stringify(entries))
end)
