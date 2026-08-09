-- Declare a `search-index` generator: a script-defined generator that
-- aggregates every symbol into a single search-index.json, the kind of
-- artifact the per-page generators cannot produce.
--
-- `mrdocs.register_generator(id, fn)` declares it next to any
-- `mrdocs.register_transform` a script might also declare; selecting
-- `generator: <id>` runs `fn` with one `ctx`. `ctx.corpus.symbols` is
-- every symbol (each reflecting its `id`, the base58 SymbolID string, so
-- the generator can form stable per-symbol URLs) and `ctx.output.write`
-- emits files under the output directory.

-- Quote a string as a JSON value.
local function json_string(s)
  s = s:gsub('\\', '\\\\'):gsub('"', '\\"')
  return '"' .. s .. '"'
end

mrdocs.register_generator("search-index", function(ctx)
  local entries = {}
  for _, sym in ipairs(ctx.corpus.symbols) do
    local name = sym.name or ""
    if name ~= "" then
      entries[#entries + 1] =
        '{"name":' .. json_string(name) ..
        ',"url":' .. json_string(sym.id .. ".html") .. "}"
    end
  end
  ctx.output.write(
    "search-index.json",
    "[" .. table.concat(entries, ",") .. "]")
end)
