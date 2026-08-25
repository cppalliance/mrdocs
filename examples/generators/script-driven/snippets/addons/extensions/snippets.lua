-- One extension that keeps documentation examples and standalone snippet files
-- in sync, in both directions. A project picks the direction that fits how it
-- maintains its examples; some prefer one, some the other, and the two compose.
--
--   Export (comments -> files): a generator that walks the corpus and writes
--   every @code example out to a file the build can compile, so an example in a
--   comment never silently goes stale.
--
--   Import (files -> comments): a transform that reads example files kept next
--   to the project and injects them into the matching symbol's docs, so a team
--   that maintains examples as real, compiled .cpp files still gets them
--   rendered in the reference.
--
-- Because a transform runs before any generator, selecting both
-- (generator: [adoc, snippets]) imports the file-backed examples and then
-- exports every example in one pass, round-tripping the two representations.
-- The transform reads files, so the extension is written in Lua.

--- The scope path of a symbol as a list of names, outermost first, so both the
--- import and export sides map a symbol to the same `<scope>/<name>.cpp` path.
--- For `app::add` this returns `{"app", "add"}`.
--- @param ctx table The generator context, used to walk `parent` ids.
--- @param sym table The symbol whose scope path to build.
--- @return string[] parts The enclosing names then the symbol's own name.
local function scope_parts(ctx, sym)
  local parts = {}
  local cur = sym
  while cur and cur.name ~= nil do
    table.insert(parts, 1, cur.name)
    cur = cur.parent and ctx.corpus.get(cur.parent) or nil
  end
  return parts
end

--- Read a file and return its contents with trailing whitespace stripped, or
--- nil when the file does not exist. The import transform calls this for every
--- symbol, so a missing example file is an expected "nothing to import", not an
--- error.
--- @param path string The file to read.
--- @return string|nil content The trimmed contents, or nil if absent.
local function read_file(path)
  local f = io.open(path, "r")
  if not f then
    return nil
  end
  local content = f:read("*a")
  f:close()
  return (content:gsub("%s+$", ""))
end

-- Import: pull each symbol's example from <source>/<scope>/<name>.cpp into its
-- documentation as a code block.
mrdocs.register_transform("snippets", function(ctx)
  local root = (ctx.params and ctx.params.source) or "example-code"
  for _, sym in ipairs(ctx.corpus.symbols) do
    if sym.name ~= nil then
      local path = root .. "/" .. table.concat(scope_parts(ctx, sym), "/") .. ".cpp"
      local code = read_file(path)
      if code then
        if not sym.doc then
          sym.doc = {}
        end
        -- Set the description to the file's example when the symbol has none of
        -- its own; the file is the reference prose for these symbols. (A script
        -- assigns a fresh block list; it cannot append to an existing one.)
        if #(sym.doc.document or {}) == 0 then
          sym.doc.document = { { kind = "code", literal = code } }
        end
      end
    end
  end
end)

-- Export: write each symbol's @code examples out to <scope>/<name>.cpp under
-- the output directory, one file per documented symbol that has an example.
mrdocs.register_generator("snippets", function(ctx)
  for _, sym in ipairs(ctx.corpus.symbols) do
    if sym.name ~= nil and sym.doc then
      local pieces = {}
      for _, b in ipairs(sym.doc.document or {}) do
        if b.kind == "code" then
          pieces[#pieces + 1] = b.literal
        end
      end
      if #pieces > 0 then
        local path = table.concat(scope_parts(ctx, sym), "/") .. ".cpp"
        ctx.output.write(path, table.concat(pieces, "\n\n") .. "\n")
      end
    end
  end
end)
