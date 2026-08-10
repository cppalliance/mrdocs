local read_file, doc_path, paragraphs, blocks, inlines

mrdocs.register_transform("ghostwriter", function(ctx)
  local root = (ctx.params and ctx.params.source) or "external-docs"
  for _, sym in ipairs(ctx.corpus.symbols) do
    -- Fill the description from the file when the symbol has none of its own.
    -- The brief written in the header is a separate field, so it is kept; the
    -- writer's file supplies the detailed description the developer left out.
    if sym.name ~= nil and (not sym.doc or #(sym.doc.document or {}) == 0) then
      local text = read_file(doc_path(ctx, sym, root))
      if text then
        local new_blocks = blocks(paragraphs(text))
        if #new_blocks > 0 then
          if not sym.doc then
            sym.doc = {}
          end
          sym.doc.document = new_blocks
        end
      end
    end
  end
end)

--- Read a whole file into a string.
--- @param path string Path to the file, relative to the run's working directory.
--- @return string|nil The file's contents, or nil when the file does not exist.
function read_file(path)
  local f = io.open(path, "r")
  if not f then
    return nil
  end
  local content = f:read("*a")
  f:close()
  return content
end

--- The external file that documents a symbol, mirroring its scope.
--- For example app::Vec2 maps to `<root>/app/Vec2.md`.
--- @param ctx table The transform context, used to walk `parent` ids.
--- @param sym table The symbol to locate a file for.
--- @param root string The source directory holding the writer's files.
--- @return string The path to the symbol's Markdown file.
function doc_path(ctx, sym, root)
  local parts = {}
  local cur = sym
  while cur and cur.name ~= nil do
    table.insert(parts, 1, cur.name)
    cur = cur.parent and ctx.corpus.get(cur.parent) or nil
  end
  return root .. "/" .. table.concat(parts, "/") .. ".md"
end

--- Split Markdown text into paragraphs.
--- Paragraphs are separated by blank lines; a soft line break inside a paragraph
--- is collapsed to a single space.
--- @param text string The raw Markdown.
--- @return string[] One entry per non-empty paragraph.
function paragraphs(text)
  local out = {}
  text = text:gsub("\r\n", "\n")
  for para in (text .. "\n\n"):gmatch("(.-)\n\n") do
    para = para:gsub("%s*\n%s*", " "):gsub("^%s+", ""):gsub("%s+$", "")
    if #para > 0 then
      out[#out + 1] = para
    end
  end
  return out
end

--- Wrap each paragraph string in a documentation paragraph block.
--- @param paras string[] Paragraphs, as returned by `paragraphs`.
--- @return table[] Paragraph block nodes, suitable for `sym.doc.document`.
function blocks(paras)
  local out = {}
  for _, p in ipairs(paras) do
    out[#out + 1] = { kind = "paragraph", children = inlines(p) }
  end
  return out
end

--- Parse one paragraph into documentation inline nodes.
--- Text wrapped in single backticks becomes an inline-code node; everything else
--- becomes plain text. Only backtick code spans are handled here; a richer
--- transform could parse more of Markdown.
--- @param text string A single paragraph, with no line breaks.
--- @return table[] Inline nodes, suitable as a block's `children`.
function inlines(text)
  local nodes = {}
  local is_code = false
  for segment in (text .. "`"):gmatch("(.-)`") do
    if #segment > 0 then
      if is_code then
        nodes[#nodes + 1] = { kind = "code", children = { { kind = "text", literal = segment } } }
      else
        nodes[#nodes + 1] = { kind = "text", literal = segment }
      end
    end
    is_code = not is_code
  end
  return nodes
end
