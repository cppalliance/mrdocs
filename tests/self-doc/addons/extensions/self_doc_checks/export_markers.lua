local helpers = require("helpers")

-- Functions that need no marker, keyed by qualified name and matched only
-- where declared in the given header, so other overloads stay checked.
local EXEMPT_FUNCTIONS = {
    ["mrdocs::loadPlugin"] = {
        header = "mrdocs/Extensions/ExtensionRegistry.hpp",
        reason = "friend grant; the loader is declared in src/, not exported",
    },
}

-- The attribute each marker is recorded under while MrDocs parses, where both
-- expand to a GNU attribute: `MRDOCS_DECL` (and its dom and handlebars twins)
-- to `visibility`; `MRDOCS_VISIBLE` and its dom and handlebars twins
-- (`MRDOCS_DOM_VISIBLE`, `MRDOCS_HANDLEBARS_VISIBLE`) to `type_visibility`.
local DECL_ATTRIBUTES = { ["visibility"] = true, ["__visibility__"] = true }
local VISIBLE_ATTRIBUTES = { ["type_visibility"] = true, ["__type_visibility__"] = true }

local has_attribute, has_decl, has_visible, is_defined_in_source, is_template
local is_exempt_function, needs_source_marker, late_static_names, class_findings
local join_names, is_internal, is_static_defined_in_source

-- Guard the export markers: every public declaration in the `mrdocs`
-- namespace that a plugin links against carries the marker it needs, and no
-- declaration carries a marker it does not need. The check needs the
-- self-documentation run to pass `--non-standard-attributes`, so the markers
-- are recorded as attributes on the declarations. MrDocs defines
-- `__MRDOCS__` when it parses, and the markers then expand the same way on
-- every platform.
--
-- The corpus holds the headers and the `.ipp` files they include, so a
-- function whose declaration has no definition location has its body in a
-- source file. No source file is read. A static data member is defined after
-- its class when its definition location is not its declaration. The needed
-- marker follows from that:
--
-- * a public class with a member function defined in a source file, public or
--   private, or a static data member defined in one, needs `MRDOCS_DECL`, on
--   the class or on each such member
-- * a class that defines a static data member after its body must not carry
--   `MRDOCS_DECL`, since MSVC rejects the definition of a `dllimport` static
--   data member (C2491); it carries `MRDOCS_VISIBLE`, and `MRDOCS_DECL` goes
--   on each member defined in a source file
-- * every public class without a class-level `MRDOCS_DECL` carries
--   `MRDOCS_VISIBLE`. The macro is empty on MSVC and gives the class default
--   visibility on GCC and Clang, so a member that lacks its own
--   `MRDOCS_DECL` still links there and fails only on MSVC. The self-doc
--   parse records it as `type_visibility`, which is why the check asks for it
-- * a free function defined in a source file needs `MRDOCS_DECL`
-- * `MRDOCS_VISIBLE` goes only on classes; on a member or a function it is a
--   parse error ("attribute only applies to types and namespaces"), so the
--   run fails before this check and no verdict is needed
--
-- Each of the six ways a marker can be wrong is an error that names the
-- symbol (a member as `<class>::<member>`) and the marker it expects:
-- nothing where `MRDOCS_DECL` or `MRDOCS_VISIBLE` is needed,
-- `MRDOCS_DECL` where nothing or `MRDOCS_VISIBLE` is needed, and
-- `MRDOCS_VISIBLE` where nothing or `MRDOCS_DECL` is needed. A suggestion
-- names only a placement that is valid: a member of a class that defines a
-- static data member after its body, or in a class where other members
-- carry the marker (moving it to the class then is MSVC error C2487), is told
-- to carry the marker itself. A class with both markers is an error too.
--
-- The rule skips what never needs a marker:
--
-- * templates and members of templates, which are instantiated in the plugin;
--   an explicit specialization is not a template and is checked
-- * inline and `constexpr` functions, which always have a body in the header
-- * deleted, defaulted and pure virtual functions, except a pure virtual
--   destructor, whose body every derived destructor calls
-- * `static` free functions, which have internal linkage
-- * inherited members, which belong to the base class
-- * the functions listed in `EXEMPT_FUNCTIONS`
-- * types in the `polyfill` and `describe` namespaces
-- * types in a `detail` namespace, private and protected types, classes
--   whose definition is not in the parsed headers, declarations under `src/`
--   or in a source file, and private and protected
--   members as declarations of their own, which need no marker;
--   `MRDOCS_VISIBLE` on one of them is reported
--
-- The check runs only when the configuration extracts private members (the
-- `mrdocs-self-doc-private` test), because a private member function with its
-- body in a source file is called by the inline functions of its class and
-- needs the marker as much as a public one. Private members count when
-- deciding what the class needs.
-- The `export-markers` test in `tests/self-doc/CMakeLists.txt` runs the check
-- over the cases in `tests/self-doc/export-markers`.
mrdocs.register_transform("export-markers", function(ctx)
    if not ctx.config.extractPrivate then
        return
    end
    local by_id = helpers.index_by_id(ctx)
    local members = {}
    local statics = {}
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "function" and sym.parent then
            members[sym.parent] = members[sym.parent] or {}
            table.insert(members[sym.parent], sym)
        elseif sym.kind == "variable" and sym.parent and sym.storageClass == "static" then
            statics[sym.parent] = statics[sym.parent] or {}
            table.insert(statics[sym.parent], sym)
        end
    end
    local template_names = {}
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.kind == "record" and sym.template and sym.name ~= "" then
            template_names[helpers.qualified_name(by_id, sym)] = true
        end
    end
    local reported = {}
    local function report(name, message)
        local text = name .. ": " .. message
        if not reported[text] then
            reported[text] = true
            ctx.report.error(text)
        end
    end
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.extraction == "regular"
            and not sym.inheritedFrom
            and sym.name and sym.name ~= ""
            and (sym.kind == "function" or sym.kind == "record") then
            local qualified = helpers.qualified_name(by_id, sym)
            if qualified:sub(1, 8) == "mrdocs::"
                and not helpers.is_exempt_namespace(qualified) then
                local detail = helpers.is_implementation_detail(by_id, sym)
                    or is_internal(sym)
                if sym.kind == "record" then
                    -- A class template with a requires clause is extracted
                    -- twice, and the second symbol is its pattern, which has
                    -- no template information.
                    local template = template_names[qualified]
                        or is_template(by_id, sym)
                    if detail or template then
                        if has_visible(sym) then
                            report(qualified, "has MRDOCS_VISIBLE and needs no marker, since "
                                .. (template and "it is a template, instantiated in the plugin"
                                    or "it is not public API"))
                        end
                    else
                        for _, finding in ipairs(class_findings(
                                by_id, members, statics, sym, qualified)) do
                            report(finding.name, finding.message)
                        end
                    end
                else
                    local parent = by_id[sym.parent]
                    if not detail
                        and not (parent and parent.kind == "record")
                        and not is_exempt_function(qualified, sym)
                        and sym.storageClass ~= "static" then
                        local needs = needs_source_marker(by_id, sym)
                        if needs and not has_decl(sym) then
                            report(qualified, "has no marker and needs MRDOCS_DECL, "
                                .. "since its body is in a source file")
                        elseif has_decl(sym) and sym.loc.defLoc ~= nil
                            and not is_template(by_id, sym) then
                            report(qualified, "has MRDOCS_DECL and needs no marker, "
                                .. "since its body is in a header")
                        end
                    end
                end
            end
        end
    end
end)

--- Whether a declaration records an attribute from a set.
--- @param sym table The symbol to check.
--- @param names table The attribute names, as a set.
--- @return boolean True when one of the attributes is recorded.
function has_attribute(sym, names)
    for _, attribute in ipairs(sym.attributes or {}) do
        if names[attribute.name] then
            return true
        end
    end
    return false
end

--- Whether a declaration carries `MRDOCS_DECL`.
--- @param sym table The symbol to check.
--- @return boolean True when the visibility attribute is recorded.
function has_decl(sym)
    return has_attribute(sym, DECL_ATTRIBUTES)
end

--- Whether a declaration carries `MRDOCS_VISIBLE`.
--- @param sym table The symbol to check.
--- @return boolean True when the type visibility attribute is recorded.
function has_visible(sym)
    return has_attribute(sym, VISIBLE_ATTRIBUTES)
end

--- Whether a function is listed in `EXEMPT_FUNCTIONS` and declared in the
--- header its entry names.
--- @param qualified string The qualified name of the function.
--- @param sym table The function to check.
--- @return boolean True when the function needs no marker.
function is_exempt_function(qualified, sym)
    local entry = EXEMPT_FUNCTIONS[qualified]
    if not entry then
        return false
    end
    for _, location in ipairs(sym.loc.loc) do
        if location.shortPath == entry.header then
            return true
        end
    end
    return false
end

--- Whether a function has no definition among the parsed files, so its body
--- is in a source file.
--- @param sym table The function to check.
--- @return boolean True when no definition location is recorded.
function is_defined_in_source(sym)
    return sym.loc.defLoc == nil
end

--- Whether a symbol or one of its enclosing records is a template. A full
--- specialization (`template <> ...`) names a primary and has no parameters
--- of its own: it is an ordinary declaration, not a template.
--- @param by_id table Symbols keyed by id.
--- @param sym table The symbol to check.
--- @return boolean True when the symbol is instantiated where it is used.
function is_template(by_id, sym)
    local current = sym
    while current and current.name and current.name ~= "" do
        local template = current.template
        if template and not (template.primary and #template.params == 0) then
            return true
        end
        current = by_id[current.parent]
    end
    return false
end

--- Whether a function has its body in a source file and is not one of the
--- functions that never need a marker.
--- @param by_id table Symbols keyed by id.
--- @param sym table The function to check.
--- @return boolean True when the function needs `MRDOCS_DECL`.
function needs_source_marker(by_id, sym)
    return is_defined_in_source(sym)
        and not sym.isDeleted
        and not sym.isDefaulted
        and not (sym.isPure and sym.name:sub(1, 1) ~= "~")
        and not is_template(by_id, sym)
end

--- Whether a declaration is out of reach of a plugin: a class whose
--- definition is not in the parsed headers (a forward declaration is all a
--- plugin sees), or a declaration under `src/` or in a source file.
--- @param sym table The symbol to check.
--- @return boolean True when the declaration is internal to MrDocs.
function is_internal(sym)
    local location = sym.loc.defLoc
    if sym.kind ~= "record" then
        location = sym.loc.loc[1] or location
    end
    if not location then
        return true
    end
    local path = location.sourcePath or ""
    return path:sub(1, 4) == "src/" or path:match("%.cpp$") ~= nil
end

--- Whether a static data member is defined in a source file: it has no
--- definition among the parsed files and is not `constexpr` or `inline`.
--- @param member table The static data member to check.
--- @return boolean True when the definition is in a source file.
function is_static_defined_in_source(member)
    return member.loc.defLoc == nil
        and not member.isConstexpr
        and not member.isInline
end

--- The names joined for a message: `a`, `a and b`, `a, b and c`.
--- @param names table The names, in order.
--- @return string The joined names.
function join_names(names)
    if #names <= 1 then
        return names[1] or ""
    end
    return table.concat(names, ", ", 1, #names - 1) .. " and " .. names[#names]
end

--- The names of the static data members a class defines after its body: the
--- declaration is in the class and the definition is elsewhere, as in
--- `constexpr inline SymbolID SymbolID::invalid = SymbolID();`. A definition
--- inside the class is the only location the member records.
--- @param statics table Static data members keyed by the id of their class.
--- @param sym table The class to check.
--- @return table The names of the members, in declaration order.
function late_static_names(statics, sym)
    local names = {}
    for _, member in ipairs(statics[sym.id] or {}) do
        local def = member.loc.defLoc
        if def and not member.inheritedFrom then
            local at_declaration = member.loc.loc[1] == nil
            for _, location in ipairs(member.loc.loc) do
                if location.shortPath == def.shortPath
                    and location.lineNumber == def.lineNumber then
                    at_declaration = true
                end
            end
            if not at_declaration then
                table.insert(names, member.name)
            end
        end
    end
    return names
end

--- The findings for a public class: what its markers and the markers of its
--- members get wrong. Each is a symbol name and a message.
--- @param by_id table Symbols keyed by id.
--- @param members table Member functions keyed by the id of their class.
--- @param statics table Static data members keyed by the id of their class.
--- @param sym table The class to check.
--- @param qualified string The qualified name of the class.
--- @return table The findings, each with `name` and `message`.
function class_findings(by_id, members, statics, sym, qualified)
    local findings = {}
    local function add(name, message)
        table.insert(findings, { name = name, message = message })
    end
    -- The members that need MRDOCS_DECL: functions with their body in a
    -- source file, and static data members defined in one.
    local source, unmarked, inline_marked = {}, {}, {}
    for _, member in ipairs(members[sym.id] or {}) do
        if not member.inheritedFrom then
            if needs_source_marker(by_id, member) then
                table.insert(source, { name = member.name, what = "body" })
                if not has_decl(member) then
                    table.insert(unmarked, { name = member.name, what = "body" })
                end
            elseif has_decl(member) and member.loc.defLoc ~= nil
                and not is_template(by_id, member) then
                table.insert(inline_marked, member)
            end
        end
    end
    for _, member in ipairs(statics[sym.id] or {}) do
        if not member.inheritedFrom and is_static_defined_in_source(member) then
            table.insert(source, { name = member.name, what = "definition" })
            if not has_decl(member) then
                table.insert(unmarked, { name = member.name, what = "definition" })
            end
        end
    end
    local decl, visible = has_decl(sym), has_visible(sym)
    local late = late_static_names(statics, sym)
    local source_names = {}
    for _, entry in ipairs(source) do
        table.insert(source_names, entry.name)
    end
    local source_text = join_names(source_names)
    local verb = #source == 1 and "is" or "are"
    local decl_members = #source - #unmarked
    if decl and visible then
        local only_decl = #late == 0 and #source > 0
        add(qualified, "has both MRDOCS_DECL and MRDOCS_VISIBLE and needs only "
            .. (only_decl and "MRDOCS_DECL" or "MRDOCS_VISIBLE"))
        if only_decl then
            visible = false
        else
            decl = false
        end
    end
    -- A member is told to carry the marker itself. Offering the class
    -- would be wrong when the class defines a static data member after its
    -- body, and when other members already carry the marker, because moving
    -- it to the class then is MSVC error C2487. When no member carries it,
    -- the class is told directly instead.
    local function member_missing(entry)
        add(qualified .. "::" .. entry.name,
            "its " .. entry.what .. " is in a source file, so it needs MRDOCS_DECL on itself")
    end
    if decl then
        if #late > 0 then
            local verb_late = #late == 1 and "is" or "are"
            add(qualified, "MRDOCS_DECL on the class breaks MSVC, since "
                .. join_names(late) .. " " .. verb_late .. " defined after the class; put "
                .. (#source > 0 and ("MRDOCS_DECL on " .. source_text .. " and ") or "")
                .. "MRDOCS_VISIBLE on the class instead")
        elseif #source == 0 then
            add(qualified, "has MRDOCS_DECL and needs MRDOCS_VISIBLE, "
                .. "since no member is defined in a source file")
        else
            for _, member in ipairs(members[sym.id] or {}) do
                if not member.inheritedFrom and has_decl(member)
                    and needs_source_marker(by_id, member) then
                    add(qualified .. "::" .. member.name,
                        "has MRDOCS_DECL and needs no marker, since " .. qualified
                        .. " already has it")
                end
            end
            for _, member in ipairs(statics[sym.id] or {}) do
                if not member.inheritedFrom and has_decl(member)
                    and is_static_defined_in_source(member) then
                    add(qualified .. "::" .. member.name,
                        "has MRDOCS_DECL and needs no marker, since " .. qualified
                        .. " already has it")
                end
            end
        end
    elseif visible then
        if #source > 0 and #unmarked == #source and #late == 0 then
            add(qualified, "has MRDOCS_VISIBLE and needs MRDOCS_DECL, since "
                .. source_text .. " " .. verb .. " defined in a source file")
        else
            for _, entry in ipairs(unmarked) do
                member_missing(entry)
            end
        end
    else
        if #source > 0 and decl_members == 0 and #late == 0 then
            add(qualified, "has no marker and needs MRDOCS_DECL, since "
                .. source_text .. " " .. verb .. " defined in a source file")
        else
            add(qualified, "has no marker and needs MRDOCS_VISIBLE")
            for _, entry in ipairs(unmarked) do
                member_missing(entry)
            end
        end
    end
    for _, member in ipairs(inline_marked) do
        add(qualified .. "::" .. member.name,
            "has MRDOCS_DECL and needs no marker, since its body is in a header")
    end
    return findings
end
