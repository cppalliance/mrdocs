local read_file, header_of, stem_of, line_of, dedent, extract_snippets
local to_plain, code_blocks, indent_block, unique_name, symbol_function
local fingerprint, mark_imported, was_imported

-- Fingerprints of the snippets the transform imported, kept so the generator can
-- skip re-exporting an example that already lives in a compiled file. This is
-- module-level state shared between the two callbacks: the transform fills it,
-- the generator reads it.
local imported = {}

-- Import: for each symbol with no example of its own, append the snippets tagged
-- with its name in `<source>/<header-stem>.cpp` to its documentation, after any
-- description it already carries, and remember them so export skips them.
mrdocs.register_transform("snippets", function(ctx)
  local root = (ctx.params and ctx.params.source) or "example-code"
  local files = {}
  for _, sym in ipairs(ctx.corpus.symbols) do
    if sym.name ~= nil and #code_blocks(sym) == 0 then
      local header = header_of(sym)
      if header then
        local path = root .. "/" .. stem_of(header) .. ".cpp"
        if files[path] == nil then
          files[path] = read_file(path) or false
        end
        local text = files[path]
        if text then
          local snippets = extract_snippets(text, sym.name)
          if #snippets > 0 then
            if not sym.doc then
              sym.doc = {}
            end
            -- A script assigns a fresh block list rather than mutating one in
            -- place, so rebuild the description and append the imported snippets.
            local doc = {}
            for _, b in ipairs(sym.doc.document or {}) do
              doc[#doc + 1] = to_plain(b)
            end
            -- MrDocs heads an inline @code example with an "Example"/"Examples"
            -- heading; add the same before the imported snippets so they read
            -- the same as inline ones.
            local label = (#snippets == 1) and "Example" or "Examples"
            doc[#doc + 1] = {
              kind = "heading",
              children = { { kind = "text", literal = label } },
            }
            for _, snippet in ipairs(snippets) do
              doc[#doc + 1] = { kind = "code", literal = snippet }
              mark_imported(snippet, header)
            end
            sym.doc.document = doc
          end
        end
      end
    end
  end
end)

-- Export: group every symbol's inline @code examples by header and write one
-- runnable test file per header under `<output>/<header-stem>.cpp`, one function
-- per symbol plus a `main` that calls them all.
mrdocs.register_generator("snippets", function(ctx)
  local out_dir = (ctx.params and ctx.params.output) or "exported"
  local groups = {}
  local order = {}
  for _, sym in ipairs(ctx.corpus.symbols) do
    if sym.name ~= nil then
      local header = header_of(sym)
      if header then
        -- Export only the snippets written inline in the header; the ones the
        -- transform pulled from example files are already tested where they live.
        local blocks = {}
        for _, literal in ipairs(code_blocks(sym)) do
          if not was_imported(literal, header) then
            blocks[#blocks + 1] = literal
          end
        end
        if #blocks > 0 then
          if not groups[header] then
            groups[header] = {}
            order[#order + 1] = header
          end
          local g = groups[header]
          g[#g + 1] = { name = sym.name, line = line_of(sym), literals = blocks }
        end
      end
    end
  end
  for _, header in ipairs(order) do
    local syms = groups[header]
    table.sort(syms, function(a, b) return a.line < b.line end)
    local seen = {}
    local parts = { "#include \"" .. header .. "\"\n#include <cassert>\n" }
    local calls = {}
    for _, sym in ipairs(syms) do
      local fn, def = symbol_function(sym.name, sym.literals, seen)
      parts[#parts + 1] = def
      calls[#calls + 1] = "    " .. fn .. "();"
    end
    parts[#parts + 1] = "int main()\n{\n" .. table.concat(calls, "\n") .. "\n}\n"
    ctx.output.write(out_dir .. "/" .. stem_of(header) .. ".cpp", table.concat(parts, "\n"))
  end
end)

--- The code-block literals in a symbol's documentation, in order. A `@code`
--- example, whether written inline or imported, is a `code` block in the
--- symbol's `document`.
--- @param sym table The symbol to read.
--- @return string[] The code literals.
function code_blocks(sym)
  local out = {}
  if sym.doc then
    for _, b in ipairs(sym.doc.document or {}) do
      if b.kind == "code" then
        out[#out + 1] = b.literal
      end
    end
  end
  return out
end

--- The header a symbol lives in, as a short path. A defined entity carries a
--- definition location; one that is only declared (like these functions) has
--- an empty `defLoc`, so fall back to the first declaration in `loc`. Symbols
--- are grouped by this so each header maps to one input and one output example
--- file.
--- @param sym table The symbol whose header to read.
--- @return string|nil header The header path, or nil if unknown.
function header_of(sym)
  local info = sym.loc
  if not info then
    return nil
  end
  local def = info.defLoc
  if def and def.shortPath and #def.shortPath > 0 then
    return def.shortPath
  end
  for _, decl in ipairs(info.loc or {}) do
    if decl.shortPath and #decl.shortPath > 0 then
      return decl.shortPath
    end
  end
  return nil
end

--- The file stem of a header path: `arithmetic.hpp` -> `arithmetic`, so a
--- header maps to a `<stem>.cpp` example file next to it in each directory.
--- @param header string The header path.
--- @return string stem The stem, extension and directory removed.
function stem_of(header)
  local base = header:match("([^/]+)$") or header
  return base:gsub("%.[^.]*$", "")
end

--- Read a file and return its contents, or nil when the file does not exist.
--- @param path string The file to read.
--- @return string|nil content The contents, or nil if absent.
function read_file(path)
  local f = io.open(path, "r")
  if not f then
    return nil
  end
  local content = f:read("*a")
  f:close()
  return content
end

--- Extract every snippet tagged `name` from an example file's text, in order.
--- A snippet is the dedented lines between `//[<name>` and the next `//]`, so a
--- symbol documented by several regions collects several snippets.
--- @param text string The example file's contents.
--- @param name string The snippet tag, a symbol name.
--- @return string[] The snippets, one per tagged region.
function extract_snippets(text, name)
  local snippets = {}
  local body = nil
  for line in (text .. "\n"):gmatch("(.-)\n") do
    if body then
      if line:match("^%s*//%]%s*$") then
        snippets[#snippets + 1] = table.concat(dedent(body), "\n")
        body = nil
      else
        body[#body + 1] = line
      end
    elseif line:match("^%s*//%[" .. name .. "%s*$") then
      body = {}
    end
  end
  return snippets
end

--- Strip the common leading indentation from a list of lines, so an extracted
--- snippet reads as top-level code rather than carrying the indentation it had
--- inside its enclosing function.
--- @param body_lines string[] The snippet's lines.
--- @return string[] The lines with the shared indent removed.
function dedent(body_lines)
  local indent = nil
  for _, line in ipairs(body_lines) do
    if line:match("%S") then
      local lead = #line - #(line:gsub("^%s+", ""))
      if indent == nil or lead < indent then
        indent = lead
      end
    end
  end
  if not indent or indent == 0 then
    return body_lines
  end
  local out = {}
  for _, line in ipairs(body_lines) do
    out[#out + 1] = line:sub(indent + 1)
  end
  return out
end

--- Deep-copy a documentation value into plain Lua tables. A block read back
--- from the corpus is a live proxy that the engine will not accept as a fresh
--- value, so rebuilding a symbol's `document` means converting the blocks it
--- already has, dropping the internal `$meta` key and any methods.
--- @param value any A DOM value: a primitive, an array, or an object.
--- @return any A plain copy safe to assign back to `document`.
function to_plain(value)
  local t = type(value)
  if t ~= "table" and t ~= "userdata" then
    return value
  end
  if value[1] ~= nil then
    local arr = {}
    local i = 1
    while value[i] ~= nil do
      arr[i] = to_plain(value[i])
      i = i + 1
    end
    return arr
  end
  local obj = {}
  for k, v in pairs(value) do
    if type(k) == "string" and k ~= "$meta" and type(v) ~= "function" then
      obj[k] = to_plain(v)
    end
  end
  return obj
end

--- Record that a snippet was imported from an example file.
--- @param literal string The imported snippet.
--- @param header string The header its symbol belongs to.
function mark_imported(literal, header)
  imported[fingerprint(literal, header)] = true
end

--- A fingerprint for a snippet: the header it belongs to, its byte size, and a
--- hash of its text, joined into one key. Import and export both know a symbol's
--- header and its snippet text, so both compute the same key for the same
--- snippet, without either side storing the text itself.
--- @param literal string The snippet code.
--- @param header string The header the snippet's symbol belongs to.
--- @return string key The fingerprint.
function fingerprint(literal, header)
  local hash = 5381
  for i = 1, #literal do
    hash = (hash * 33 + literal:byte(i)) % 2147483648
  end
  return header .. "|" .. #literal .. "|" .. hash
end

--- Whether a snippet was imported, and so should not be exported again: an
--- imported example is already a tested file, so writing it back out would only
--- test it twice.
--- @param literal string The snippet.
--- @param header string The header its symbol belongs to.
--- @return boolean True when the snippet came from an example file.
function was_imported(literal, header)
  return imported[fingerprint(literal, header)] == true
end

--- The source line a symbol appears on, used to order exported snippets the way
--- they read in the header. Falls back to the first declaration, like
--- `header_of`, and to 0 when unknown.
--- @param sym table The symbol whose line to read.
--- @return number line The 1-based line number, or 0 if unknown.
function line_of(sym)
  local info = sym.loc
  if not info then
    return 0
  end
  if info.defLoc and (info.defLoc.lineNumber or 0) > 0 then
    return info.defLoc.lineNumber
  end
  for _, decl in ipairs(info.loc or {}) do
    if (decl.lineNumber or 0) > 0 then
      return decl.lineNumber
    end
  end
  return 0
end

--- The test function for a symbol: `void <name>_snippet() { ... }`. A single
--- snippet is the body; several are each placed in their own block scope so
--- their declarations do not collide.
--- @param sym_name string The symbol name, used for the function name.
--- @param literals string[] The symbol's snippets, in order.
--- @param seen table<string, boolean> Function names already used in this file.
--- @return string fn The function name.
--- @return string def The function definition.
function symbol_function(sym_name, literals, seen)
  local fn = unique_name(sym_name .. "_snippet", seen)
  local body
  if #literals == 1 then
    body = indent_block(literals[1], 4)
  else
    local scopes = {}
    for _, literal in ipairs(literals) do
      scopes[#scopes + 1] = "    {\n" .. indent_block(literal, 8) .. "\n    }"
    end
    body = table.concat(scopes, "\n")
  end
  return fn, "void " .. fn .. "()\n{\n" .. body .. "\n}\n"
end

--- A function name derived from `base`, made unique within `seen` by appending
--- a counter, so two symbols with the same name never define the same function.
--- @param base string The preferred name.
--- @param seen table<string, boolean> Names already used in this file.
--- @return string The unique name.
function unique_name(base, seen)
  local name = base
  local n = 1
  while seen[name] do
    n = n + 1
    name = base .. "_" .. n
  end
  seen[name] = true
  return name
end

--- Indent every non-empty line of a snippet by `n` spaces.
--- @param literal string The snippet code.
--- @param n number The number of leading spaces.
--- @return string The indented snippet.
function indent_block(literal, n)
  local pad = string.rep(" ", n)
  local out = {}
  for line in (literal .. "\n"):gmatch("(.-)\n") do
    out[#out + 1] = (#line > 0) and (pad .. line) or ""
  end
  return table.concat(out, "\n")
end
