-- Quote a string as a JSON value.
local function json_string(s)
  s = s:gsub('\\', '\\\\'):gsub('"', '\\"')
  return '"' .. s .. '"'
end

function generate(corpus, output)
  local entries = {}
  for _, sym in ipairs(corpus.symbols) do
    local name = sym.name or ""
    if name ~= "" then
      entries[#entries + 1] =
        '{"name":' .. json_string(name) ..
        ',"url":' .. json_string(sym._id .. ".html") .. "}"
    end
  end
  output.write(
    "search-index.json",
    "[" .. table.concat(entries, ",") .. "]")
end
