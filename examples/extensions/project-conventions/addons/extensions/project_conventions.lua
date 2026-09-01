local node_text, symbol_brief, is_low_quality_brief

-- Check each symbol's documentation against the project's conventions. Every
-- convention is a small predicate; this transform warns about each symbol that
-- breaks one, routing through mrdocs.report so the messages count as real
-- diagnostics. The one convention checked here flags a "low-quality brief": a
-- brief that reads like a stray comment rather than a description.
mrdocs.register_transform("project-conventions", function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        local brief = symbol_brief(sym)
        if is_low_quality_brief(brief) then
            mrdocs.report.warn(sym.name .. ': low-quality brief: "' .. brief .. '"')
        end
    end
end)

--- The text of a documentation node, gathered by walking its children.
--- @param node table A documentation node with `literal` and `children`.
--- @return string The concatenated text of the node and its descendants.
function node_text(node)
    local text = node.literal or ""
    if node.children then
        for _, child in ipairs(node.children) do
            text = text .. node_text(child)
        end
    end
    return text
end

--- A symbol's brief as plain text, trimmed, or "" when it has none.
--- @param sym table The symbol to read.
--- @return string The brief text, or an empty string.
function symbol_brief(sym)
    if not (sym.doc and sym.doc.brief) then
        return ""
    end
    return node_text(sym.doc.brief):match("^%s*(.-)%s*$")
end

--- Whether a brief is low-quality: it reads like a stray comment rather than a
--- description because it is too short or more punctuation than words.
--- @param brief string The brief text, already trimmed.
--- @return boolean True when the brief breaks the convention.
function is_low_quality_brief(brief)
    if #brief == 0 then
        return false
    end
    local special, total = 0, 0
    for c in brief:gmatch(".") do
        if c ~= " " then
            total = total + 1
            if not c:match("%w") then special = special + 1 end
        end
    end
    return #brief < 4 or special > total / 2
end
