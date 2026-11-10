local TYPE_KINDS = { record = true, enum = true, typedef = true, concept = true }

local helpers = require("helpers")

local is_pascal_case, is_exempt

-- Guard the naming convention for types: every public record, enum, typedef
-- and concept in the `mrdocs` namespace is named in PascalCase. Types in a
-- `detail` namespace and private or protected members are implementation
-- details and are skipped. Each type that breaks the rule is reported through
-- ctx.report, so the self-documentation test (which runs with warnings as
-- errors) fails on it. Concepts, aliases, traits and container member types
-- are named like their std:: counterparts and are excused (see `is_exempt`).
mrdocs.register_transform("type-names", function(ctx)
    local by_id = helpers.index_by_id(ctx)
    local children = helpers.index_by_parent(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if TYPE_KINDS[sym.kind] and sym.name and sym.name ~= "" then
            local qualified = helpers.qualified_name(by_id, sym)
            if qualified:sub(1, 8) == "mrdocs::"
                and not is_pascal_case(sym.name)
                and not helpers.is_implementation_detail(by_id, sym)
                and not is_exempt(by_id, children, sym, qualified) then
                ctx.report.warn(qualified .. ": type names should use PascalCase")
            end
        end
    end
end)

--- Whether a name is PascalCase: it starts with an uppercase letter and has
--- no underscore, so `FileVisitor` passes and `file_visitor` does not.
--- @param name string The unqualified type name.
--- @return boolean True when the name follows the convention.
function is_pascal_case(name)
    return name:match("^%u[%w]*$") ~= nil
end

--- Whether a type is excused from the naming rule: concepts (named like the
--- std:: concepts), member aliases and `_t` aliases (named like the std::
--- aliases they mirror), types in an exempt namespace, traits, and container
--- member types.
--- @param by_id table Symbols keyed by id.
--- @param children table Symbols grouped by parent id.
--- @param sym table The symbol to check.
--- @param qualified string The qualified name of the symbol.
--- @return boolean True when the symbol is excused.
function is_exempt(by_id, children, sym, qualified)
    if sym.kind == "concept"
        or helpers.is_container_member_type(by_id, sym)
        or helpers.is_trait(by_id, children, sym) then
        return true
    end
    if sym.kind == "typedef" then
        local parent = by_id[sym.parent]
        if (parent and parent.kind == "record") or sym.name:match("_t$") then
            return true
        end
    end
    return helpers.is_exempt_namespace(qualified)
end
