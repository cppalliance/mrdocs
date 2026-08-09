//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A script-driven MrDocs generator that reflects the DOM schema from MrDocs's
// own metadata types (run MrDocs over its own headers, `--generator=schema`).
// It emits the RELAX NG XML schema, the JSON schema, and a textual reference,
// so these are no longer hand-maintained. See docs/mrdocs.yml
// (addons-supplemental) and utils/codegen/generate-schema.sh (regenerate/check).
//
// The code is organized as small top-level functions. The registered generator
// (bottom of the file) just builds a model and calls the three emitters; every
// other function is independent and documented. Each artifact is emitted in its
// own run: the JerryScript heap is small, so the model is navigated lazily and
// output is streamed with `output.append` rather than built up in memory.

/**
 * @typedef {Object} Model
 * The shared, lazily-built state passed to every helper.
 * @property {Object} ctx           The generator context (corpus, output, params).
 * @property {Object} cache         Memoized `id -> symbol` lookups.
 * @property {Array}  records       Every `mrdocs::` record symbol.
 * @property {Object} variantKinds  `base name -> [kind record]` (e.g. Symbol -> [...]).
 * @property {Object} kindToVariant `kind record id -> base name`.
 * @property {Object} neededStructIds  Set of struct ids referenced by a field.
 * @property {Object} structIds     Set of struct ids already emitted.
 * @property {Array}  structOrder   Struct ids in first-seen (emission) order.
 * @property {Object} fieldsCache   Memoized `record id -> [field]`.
 * @property {Object} singleTextCache Memoized `record id -> bool`.
 */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// Each polymorphic DOM family: the public base name mapped to the C++ common
// base its kinds derive from. Kinds of a family are documented together and
// referenced through an `Any<base>` choice.
var VARIANTS = {
  Symbol: "SymbolCommonBase", Type: "TypeCommonBase", Name: "Name",
  Attribute: "AttributeCommonBase", TArg: "TArgCommonBase", TParam: "TParamCommonBase",
  Block: "BlockCommonBase", Inline: "InlineCommonBase"
};

// Fundamental C++ types serialized as integers.
var INT_FUND = {
  "int": 1, "unsigned": 1, "unsigned int": 1, "long": 1, "unsigned long": 1, "long long": 1,
  "unsigned long long": 1, "short": 1, "unsigned short": 1, "std::size_t": 1, "size_t": 1, "std::int64_t": 1,
  "std::uint64_t": 1, "uint64_t": 1, "int64_t": 1, "char": 1, "signed char": 1, "unsigned char": 1
};

// Standard string types serialized as text.
var STRING_IDS = { "std::string": 1, "string": 1, "std::string_view": 1, "string_view": 1 };

// The reference groups, in display order: variant base name and section title.
var GROUPS = [
  ["Symbol", "Symbols"], ["Type", "Types"], ["Name", "Names"], ["Attribute", "Attributes"],
  ["TParam", "Template Parameters"], ["TArg", "Template Arguments"],
  ["Block", "Documentation Blocks"], ["Inline", "Documentation Inlines"]
];

// The fixed part of the RELAX NG grammar: the document roots and the tagfile
// vocabulary, which are not reflected from the DOM.
var RNG_PREAMBLE =
  '<?xml version="1.0" encoding="UTF-8"?>\n' +
  '<grammar xmlns="http://relaxng.org/ns/structure/1.0" datatypeLibrary="http://www.w3.org/2001/XMLSchema-datatypes">\n' +
  '  <start><choice><ref name="Mrdocs"/><ref name="Tagfile"/></choice></start>\n' +
  '  <define name="Mrdocs"><element name="mrdocs"><optional><attribute name="noNamespaceSchemaLocation" ns="http://www.w3.org/2001/XMLSchema-instance"/></optional><zeroOrMore><ref name="AnySymbol"/></zeroOrMore></element></define>\n' +
  '  <define name="Tagfile"><element name="tagfile"><oneOrMore><ref name="TagCompound"/></oneOrMore></element></define>\n' +
  '  <define name="TagCompound"><element name="compound"><attribute name="kind"><choice><value>namespace</value><value>class</value></choice></attribute><element name="name"><text/></element><element name="filename"><text/></element><zeroOrMore><choice><ref name="TagClass"/><ref name="TagMember"/></choice></zeroOrMore></element></define>\n' +
  '  <define name="TagClass"><element name="class"><attribute name="kind"><value>class</value></attribute><text/></element></define>\n' +
  '  <define name="TagMember"><element name="member"><attribute name="kind"><value>function</value></attribute><element name="type"><text/></element><element name="name"><text/></element><element name="anchorfile"><text/></element><element name="anchor"><text/></element><element name="arglist"><text/></element></element></define>\n';

// The leading, non-reflected part of the JSON Schema document.
var JSON_PREAMBLE =
  "{\n" +
  '  "$schema": "http://json-schema.org/draft-07/schema",\n' +
  '  "$id": "https://www.mrdocs.com/docs/mrdocs/master/_attachments/schemas/generators/mrdocs.schema.json",\n' +
  '  "title": "MrDocs DOM (symbols)",\n' +
  '  "type": "object",\n' +
  '  "additionalProperties": true,\n';

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

/**
 * Convert an identifier to camelCase, dropping non-alphanumeric separators.
 * @param {string} input A C++ member name, e.g. "Loc" or "is_variadic".
 * @returns {string} The camelCase form, e.g. "loc" or "isVariadic".
 */
function toCamelCase(input) {
  var out = "", up = false;
  for (var k = 0; k < input.length; ++k) {
    var c = input[k];
    if (/[A-Za-z0-9]/.test(c)) {
      if (out.length === 0) { out += c.toLowerCase(); up = false; }
      else if (up) { out += c.toUpperCase(); up = false; }
      else out += c;
    } else up = true;
  }
  return out;
}

/**
 * Remove a trailing suffix from a name, if present.
 * @param {string} name The full name.
 * @param {string} suffix The suffix to strip.
 * @returns {string} `name` without `suffix`, or `name` unchanged.
 */
function stripSuffix(name, suffix) {
  return (name.length > suffix.length && name.slice(-suffix.length) === suffix)
    ? name.slice(0, name.length - suffix.length) : name;
}

/**
 * Turn a heading into an anchor-safe slug.
 * @param {string} text e.g. "Documentation Blocks".
 * @returns {string} e.g. "documentation-blocks".
 */
function slugify(text) {
  return text.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
}

/**
 * Kebab-case an identifier, matching how enum values serialize.
 * @param {string} text e.g. "SeeBelow".
 * @returns {string} e.g. "see-below".
 */
function kebabCase(text) {
  return String(text).replace(/([a-z0-9])([A-Z])/g, "$1-$2").replace(/([A-Z]+)([A-Z][a-z])/g, "$1-$2").toLowerCase();
}

// ---------------------------------------------------------------------------
// Corpus navigation and model
// ---------------------------------------------------------------------------

/**
 * Resolve a symbol by id, memoized on the model.
 * @param {Model} model
 * @param {string} id A symbol id, or falsy.
 * @returns {Object|undefined} The symbol proxy, or undefined.
 */
function getSymbol(model, id) {
  if (!id) return undefined;
  if (id in model.cache) return model.cache[id];
  var s = model.ctx.corpus.get(id);
  model.cache[id] = s || undefined;
  return model.cache[id];
}

/**
 * Populate `model.records` with every record under the `mrdocs` namespace,
 * walking namespaces lazily so the full symbol array is never materialized.
 * @param {Model} model
 * @returns {void}
 */
function collectRecords(model) {
  var root = model.ctx.corpus.lookup("mrdocs");
  if (!root) return;
  var seen = {}, stack = [root];
  while (stack.length) {
    var ns = stack.pop();
    if (!ns || seen[ns.id]) continue;
    seen[ns.id] = true;
    var members = ns.members || {};
    var recs = members.records || [];
    for (var i = 0; i < recs.length; ++i) { var r = getSymbol(model, recs[i]); if (r) model.records.push(r); }
    var nss = members.namespaces || [];
    for (var j = 0; j < nss.length; ++j) { var n = getSymbol(model, nss[j]); if (n) stack.push(n); }
  }
}

/**
 * Every record that directly derives from a given base class name.
 * @param {Model} model
 * @param {string} baseName The C++ base class name.
 * @returns {Array} The deriving record symbols.
 */
function directDerived(model, baseName) {
  var out = [];
  for (var i = 0; i < model.records.length; ++i) {
    var bases = model.records[i].bases || [];
    for (var j = 0; j < bases.length; ++j) {
      var nm = (bases[j].type || {}).name || {};
      if (nm.identifier === baseName) { out.push(model.records[i]); break; }
    }
  }
  return out;
}

/**
 * Fill `model.variantKinds` and `model.kindToVariant` from the VARIANTS map.
 * @param {Model} model
 * @returns {void}
 */
function buildVariants(model) {
  for (var base in VARIANTS) {
    var seen = {}, list = [], derived = directDerived(model, VARIANTS[base]);
    for (var i = 0; i < derived.length; ++i) if (!seen[derived[i].id]) { seen[derived[i].id] = 1; list.push(derived[i]); }
    model.variantKinds[base] = list;
    for (var j = 0; j < list.length; ++j) model.kindToVariant[list[j].id] = base;
  }
}

/**
 * The record or enum a type refers to, resolved by id.
 * @param {Model} model
 * @param {Object} type A DOM type node.
 * @returns {Object|undefined} The referenced symbol, or undefined.
 */
function recordFromType(model, type) {
  var nm = (type || {}).name || {};
  return nm.id ? getSymbol(model, nm.id) : undefined;
}

/**
 * The i-th template argument's type of a type node.
 * @param {Object} type A DOM type node.
 * @param {number} index Zero-based argument index.
 * @returns {Object|undefined} The argument's type node, or undefined.
 */
function templateArg(type, index) {
  return (((type.name || {}).templateArgs || [])[index] || {}).type;
}

/**
 * The record base classes of a record (skipping non-record bases).
 * @param {Model} model
 * @param {Object} rec A record symbol.
 * @returns {Array} The base record symbols.
 */
function cppBaseRecords(model, rec) {
  var out = [], bases = rec.bases || [];
  for (var i = 0; i < bases.length; ++i) { var s = recordFromType(model, bases[i].type); if (s && s.kind === "record") out.push(s); }
  return out;
}

/**
 * The member variables declared directly on a record, across access levels.
 * @param {Model} model
 * @param {Object} rec A record symbol.
 * @returns {Array} The variable symbols.
 */
function variablesOf(model, rec) {
  var iface = rec.interface || {}, out = [], accs = ["public", "protected", "private"];
  for (var a = 0; a < accs.length; ++a) {
    var vs = (iface[accs[a]] || {}).variables || [];
    for (var v = 0; v < vs.length; ++v) { var s = getSymbol(model, vs[v]); if (s) out.push(s); }
  }
  return out;
}

/**
 * Every field of a record: its own variables plus inherited ones, base-first,
 * de-duplicated by name. Memoized on the model.
 * @param {Model} model
 * @param {Object} rec A record symbol.
 * @param {Object} [seen] Recursion guard of visited record ids.
 * @returns {Array} The field (variable) symbols.
 */
function allFields(model, rec, seen) {
  if (model.fieldsCache[rec.id]) return model.fieldsCache[rec.id];
  seen = seen || {};
  if (seen[rec.id]) return [];
  seen[rec.id] = true;
  var fields = [], names = {};
  var bases = cppBaseRecords(model, rec);
  for (var b = 0; b < bases.length; ++b) {
    var bf = allFields(model, bases[b], seen);
    for (var i = 0; i < bf.length; ++i) if (!names[bf[i].name]) { names[bf[i].name] = 1; fields.push(bf[i]); }
  }
  var own = variablesOf(model, rec);
  for (var j = 0; j < own.length; ++j) if (!names[own[j].name]) { names[own[j].name] = 1; fields.push(own[j]); }
  model.fieldsCache[rec.id] = fields;
  return fields;
}

/**
 * The element name a record's kind uses, e.g. "record", "typeSpecialization".
 * @param {Model} model
 * @param {Object} rec A record symbol.
 * @returns {string} The camelCase kind tag.
 */
function kindTag(model, rec) {
  var base = model.kindToVariant[rec.id];
  return toCamelCase(base ? stripSuffix(rec.name, base) : rec.name);
}

/**
 * Whether a type is a plain (non-specialization) standard string.
 * @param {Object} type A DOM type node.
 * @returns {boolean}
 */
function isStringType(type) {
  var nm = (type || {}).name || {};
  return !!STRING_IDS[nm.identifier] && nm.kind !== "specialization";
}

/**
 * Whether a record wraps a single string field (so it serializes as text).
 * @param {Model} model
 * @param {Object} rec A record symbol.
 * @returns {boolean}
 */
function isSingleTextObject(model, rec) {
  if (rec.id in model.singleTextCache) return model.singleTextCache[rec.id];
  model.singleTextCache[rec.id] = false;
  var f = allFields(model, rec);
  var res = f.length === 1 && isStringType(f[0].type);
  model.singleTextCache[rec.id] = res;
  return res;
}

/**
 * Whether a type serializes as a bare string: a string type or a single-text
 * object.
 * @param {Model} model
 * @param {Object} type A DOM type node.
 * @returns {boolean}
 */
function isStringLike(model, type) {
  if (isStringType(type)) return true;
  var r = recordFromType(model, type);
  return !!(r && r.kind === "record" && isSingleTextObject(model, r));
}

/**
 * Compute the transitive set of referenced structs in first-seen order. Seeds
 * every variant kind, then walks each struct's fields (via the RNG content
 * pass, whose side effect marks referenced structs) to a fixpoint.
 * @param {Model} model
 * @returns {void}
 */
function discoverStructs(model) {
  var needed = model.neededStructIds;
  for (var base in VARIANTS) {
    var kinds = model.variantKinds[base];
    for (var i = 0; i < kinds.length; ++i) needed[kinds[i].id] = 1;
  }
  var changed = true;
  while (changed) {
    changed = false;
    for (var id in needed) {
      if (!model.structIds[id]) {
        model.structIds[id] = 1;
        model.structOrder.push(id);
        changed = true;
        var rec = getSymbol(model, id);
        if (rec) { var f = allFields(model, rec); for (var k = 0; k < f.length; ++k) rngContentForType(model, f[k].type); }
      }
    }
  }
}

/**
 * Build the full model from a generator context.
 * @param {Object} ctx The generator context.
 * @returns {Model}
 */
function buildModel(ctx) {
  var model = {
    ctx: ctx, cache: {}, records: [], variantKinds: {}, kindToVariant: {},
    neededStructIds: {}, structIds: {}, structOrder: [], fieldsCache: {}, singleTextCache: {}
  };
  collectRecords(model);
  buildVariants(model);
  discoverStructs(model);
  return model;
}

// ---------------------------------------------------------------------------
// RELAX NG emitter
// ---------------------------------------------------------------------------

/**
 * The RELAX NG pattern for a field's type. As a side effect, marks any struct
 * it references so `discoverStructs` reaches it.
 * @param {Model} model
 * @param {Object} type A DOM type node.
 * @returns {string} A RELAX NG pattern.
 */
function rngContentForType(model, type) {
  var t = type || {}, nm = t.name || {}, id = nm.identifier;
  if (t.fundamentalType === "bool") return "<empty/>";
  if (t.fundamentalType && INT_FUND[t.fundamentalType]) return '<data type="integer"/>';
  if (id === "SymbolID") return "<text/>";
  if (isStringType(t)) return "<text/>";
  if (id === "Optional") return rngContentForType(model, templateArg(t, 0));
  if (id === "Polymorphic") { var b = ((templateArg(t, 0) || {}).name || {}).identifier; return (b && VARIANTS[b]) ? '<ref name="Any' + b + '"/>' : "<text/>"; }
  if (VARIANTS[id]) return '<ref name="Any' + id + '"/>';
  if (id === "vector" || id === "std::vector") {
    var et = templateArg(t, 0) || {}, en = (et.name || {}).identifier;
    if (en === "SymbolID" || et.fundamentalType) return "<text/>";
    if (isStringLike(model, et)) return '<zeroOrMore><element name="string"><text/></element></zeroOrMore>';
    if (en === "Polymorphic") { var bb = ((templateArg(et, 0) || {}).name || {}).identifier; if (bb && VARIANTS[bb]) return '<zeroOrMore><ref name="Any' + bb + '"/></zeroOrMore>'; }
    if (VARIANTS[en]) return '<zeroOrMore><ref name="Any' + en + '"/></zeroOrMore>';
    var er = recordFromType(model, et);
    if (er && er.kind === "record") { model.neededStructIds[er.id] = 1; return '<zeroOrMore><element name="' + kindTag(model, er) + '"><ref name="S_' + er.id + '"/></element></zeroOrMore>'; }
    return "<zeroOrMore><text/></zeroOrMore>";
  }
  var r = recordFromType(model, t);
  if (r && r.kind === "enum") return "<text/>";
  if (r && r.kind === "record") { if (isSingleTextObject(model, r)) return "<text/>"; model.neededStructIds[r.id] = 1; return '<ref name="S_' + r.id + '"/>'; }
  return "<text/>";
}

/**
 * The RELAX NG element pattern for one field.
 * @param {Model} model
 * @param {Object} field A variable symbol.
 * @returns {string}
 */
function rngFieldPattern(model, field) {
  return '<element name="' + toCamelCase(field.name) + '">' + rngContentForType(model, field.type) + "</element>";
}

/**
 * The RELAX NG body for a struct: an any-order choice over its field patterns.
 * @param {Model} model
 * @param {Object} rec A record symbol.
 * @returns {string}
 */
function rngStructBody(model, rec) {
  var f = allFields(model, rec);
  if (f.length === 0) return "<empty/>";
  var inner = "";
  for (var i = 0; i < f.length; ++i) inner += rngFieldPattern(model, f[i]);
  return "<zeroOrMore><choice>" + inner + "</choice></zeroOrMore>";
}

/**
 * The RELAX NG `Any<base>` define: a choice over the base's kind elements.
 * @param {Model} model
 * @param {string} base A variant base name.
 * @returns {string}
 */
function rngAnyDefine(model, base) {
  var kinds = model.variantKinds[base], parts = "";
  for (var i = 0; i < kinds.length; ++i) parts += '<element name="' + kindTag(model, kinds[i]) + '"><ref name="S_' + kinds[i].id + '"/></element>';
  return '<define name="Any' + base + '"><choice>' + parts + "</choice></define>";
}

/**
 * Write the RELAX NG schema: preamble, one `Any<base>` per variant, then one
 * define per referenced struct, streamed a define at a time.
 * @param {Model} model
 * @returns {void}
 */
function emitRng(model) {
  var path = "generators/mrdocs.rng", out = model.ctx.output;
  out.write(path, RNG_PREAMBLE);
  for (var base in VARIANTS) out.append(path, "  " + rngAnyDefine(model, base) + "\n");
  for (var i = 0; i < model.structOrder.length; ++i) {
    var id = model.structOrder[i];
    out.append(path, "  " + '<define name="S_' + id + '">' + rngStructBody(model, getSymbol(model, id)) + "</define>\n");
  }
  out.append(path, "</grammar>\n");
}

// ---------------------------------------------------------------------------
// JSON Schema emitter
// ---------------------------------------------------------------------------

/**
 * The JSON Schema fragment for a field's type.
 * @param {Model} model
 * @param {Object} type A DOM type node.
 * @returns {string} A JSON Schema object, as text.
 */
function jsonForType(model, type) {
  var t = type || {}, nm = t.name || {}, id = nm.identifier;
  if (t.fundamentalType === "bool") return '{"type":"boolean"}';
  if (t.fundamentalType && INT_FUND[t.fundamentalType]) return '{"type":"integer"}';
  if (id === "SymbolID") return '{"type":"string"}';
  if (isStringType(t)) return '{"type":"string"}';
  if (id === "Optional") return jsonForType(model, templateArg(t, 0));
  if (id === "Polymorphic") { var b = ((templateArg(t, 0) || {}).name || {}).identifier; return (b && VARIANTS[b]) ? '{"$ref":"#/$defs/Any' + b + '"}' : '{}'; }
  if (VARIANTS[id]) return '{"$ref":"#/$defs/Any' + id + '"}';
  if (id === "vector" || id === "std::vector") {
    var et = templateArg(t, 0) || {}, en = (et.name || {}).identifier;
    if (en === "SymbolID") return '{"type":"array","items":{"type":"string"}}';
    if (et.fundamentalType) return '{"type":"array","items":' + jsonForType(model, et) + '}';
    if (isStringLike(model, et)) return '{"type":"array","items":{"type":"string"}}';
    if (en === "Polymorphic") { var bb = ((templateArg(et, 0) || {}).name || {}).identifier; if (bb && VARIANTS[bb]) return '{"type":"array","items":{"$ref":"#/$defs/Any' + bb + '"}}'; }
    if (VARIANTS[en]) return '{"type":"array","items":{"$ref":"#/$defs/Any' + en + '"}}';
    var er = recordFromType(model, et);
    if (er && er.kind === "record") { model.neededStructIds[er.id] = 1; return '{"type":"array","items":{"$ref":"#/$defs/S_' + er.id + '"}}'; }
    return '{"type":"array"}';
  }
  var r = recordFromType(model, t);
  if (r && r.kind === "enum") return '{"type":"string"}';
  if (r && r.kind === "record") { if (isSingleTextObject(model, r)) return '{"type":"string"}'; model.neededStructIds[r.id] = 1; return '{"$ref":"#/$defs/S_' + r.id + '"}'; }
  return '{}';
}

/**
 * The JSON `$defs` entry for a variant base: an anyOf over its kinds.
 * @param {Model} model
 * @param {string} base A variant base name.
 * @returns {string}
 */
function jsonAnyDef(model, base) {
  var kinds = model.variantKinds[base], refs = [];
  for (var i = 0; i < kinds.length; ++i) refs.push('{"$ref":"#/$defs/S_' + kinds[i].id + '"}');
  return '    "Any' + base + '": {"anyOf":[' + refs.join(",") + ']}';
}

/**
 * The JSON `$defs` entry for a struct: an object with one property per field.
 * @param {Model} model
 * @param {string} id The struct id.
 * @param {Object} rec The struct's record symbol.
 * @returns {string}
 */
function jsonStructDef(model, id, rec) {
  var f = allFields(model, rec), props = [];
  for (var i = 0; i < f.length; ++i) props.push('"' + toCamelCase(f[i].name) + '":' + jsonForType(model, f[i].type));
  return '    "S_' + id + '": {"type":"object","additionalProperties":true,"properties":{' + props.join(",") + '}}';
}

/**
 * Write the JSON Schema: preamble, the `symbols` array, then the `$defs`
 * entries, streamed with separating commas so no big object is held at once.
 * @param {Model} model
 * @returns {void}
 */
function emitJson(model) {
  var path = "generators/mrdocs.schema.json", out = model.ctx.output;
  var symbolKinds = model.variantKinds.Symbol, symbolRefs = [];
  for (var i = 0; i < symbolKinds.length; ++i) symbolRefs.push('{"$ref":"#/$defs/S_' + symbolKinds[i].id + '"}');
  out.write(path, JSON_PREAMBLE +
    '  "properties": {"symbols": {"type":"array","items":{"anyOf":[' + symbolRefs.join(",") + ']}}},\n' +
    '  "$defs": {\n');
  var first = true;
  for (var base in VARIANTS) { out.append(path, (first ? "" : ",\n") + jsonAnyDef(model, base)); first = false; }
  for (var j = 0; j < model.structOrder.length; ++j) {
    var rec = getSymbol(model, model.structOrder[j]);
    if (!rec) continue;
    out.append(path, (first ? "" : ",\n") + jsonStructDef(model, model.structOrder[j], rec));
    first = false;
  }
  out.append(path, "\n  }\n}\n");
}

// ---------------------------------------------------------------------------
// AsciiDoc reference emitter
// ---------------------------------------------------------------------------

/**
 * Render inline documentation nodes to AsciiDoc, keeping code, references, and
 * emphasis (not just literal text, which mangles briefs).
 * @param {Array} nodes Inline DOM nodes.
 * @returns {string}
 */
function renderInlines(nodes) {
  var s = "";
  nodes = nodes || [];
  for (var i = 0; i < nodes.length; ++i) {
    var n = nodes[i];
    if (!n) continue;
    var k = n.kind;
    if (k === "text") s += n.literal || "";
    else if (k === "reference") { if (n.literal) s += "`" + n.literal + "`"; }
    else if (k === "code") s += "`" + renderInlines(n.children) + "`";
    else if (k === "emph") s += "_" + renderInlines(n.children) + "_";
    else if (k === "strong") s += "*" + renderInlines(n.children) + "*";
    else if (n.children) s += renderInlines(n.children);
    else if (n.literal) s += n.literal;
  }
  return s;
}

/**
 * A symbol's brief documentation as one line.
 * @param {Object} sym A symbol (with an optional `doc.brief`).
 * @returns {string} The brief, or "".
 */
function briefText(sym) {
  var b = ((sym || {}).doc || {}).brief;
  return b ? renderInlines(b.children).replace(/\s+/g, " ").trim() : "";
}

/**
 * The section anchor for a variant base (its group heading), or null.
 * @param {string} base A variant base name.
 * @returns {string|null}
 */
function groupAnchor(base) {
  for (var i = 0; i < GROUPS.length; ++i) if (GROUPS[i][0] === base) return slugify(GROUPS[i][1]);
  return null;
}

/**
 * The anchor a field links to for a concrete record: its group when it is a
 * variant kind, otherwise its own structure section.
 * @param {Model} model
 * @param {Object} rec A record symbol.
 * @returns {string}
 */
function recordAnchor(model, rec) {
  var base = model.kindToVariant[rec.id];
  return base ? groupAnchor(base) : slugify(rec.name);
}

/**
 * Wrap display text in an AsciiDoc cross-reference when an anchor is known.
 * @param {string|null} anchor The target anchor, or null.
 * @param {string} text The display text.
 * @returns {string}
 */
function linkTo(anchor, text) { return anchor ? ("<<" + anchor + "," + text + ">>") : text; }

/**
 * The bare type token for a vector element type, with "[]" appended.
 * @param {Model} model
 * @param {Object} et The element type node.
 * @returns {string}
 */
function arrayTypeCell(model, et) {
  var en = (et.name || {}).identifier;
  if (en === "SymbolID") return "string[]";
  if (et.fundamentalType) return typeCell(model, et) + "[]";
  if (isStringLike(model, et)) return "string[]";
  if (en === "Polymorphic") { var b = ((templateArg(et, 0) || {}).name || {}).identifier; if (b && VARIANTS[b]) return linkTo(groupAnchor(b), b + "[]"); }
  if (VARIANTS[en]) return linkTo(groupAnchor(en), en + "[]");
  var er = recordFromType(model, et);
  if (er && er.kind === "record") return linkTo(recordAnchor(model, er), er.name + "[]");
  return "array";
}

/**
 * The bare type token for a field: a linked type for objects and variants (with
 * "[]" for arrays), a plain scalar name otherwise. Enums and symbol ids are
 * plain strings; their constraints go in the description. The caller wraps the
 * result in backticks for monospace.
 * @param {Model} model
 * @param {Object} type A DOM type node.
 * @returns {string}
 */
function typeCell(model, type) {
  var t = type || {}, nm = t.name || {}, id = nm.identifier;
  if (t.fundamentalType === "bool") return "boolean";
  if (t.fundamentalType && INT_FUND[t.fundamentalType]) return "integer";
  if (id === "SymbolID") return "string";
  if (isStringType(t)) return "string";
  if (id === "Optional") return typeCell(model, templateArg(t, 0));
  if (id === "Polymorphic") { var b = ((templateArg(t, 0) || {}).name || {}).identifier; return (b && VARIANTS[b]) ? linkTo(groupAnchor(b), b) : "value"; }
  if (VARIANTS[id]) return linkTo(groupAnchor(id), id);
  if (id === "vector" || id === "std::vector") return arrayTypeCell(model, templateArg(t, 0) || {});
  var r = recordFromType(model, t);
  if (r && r.kind === "enum") return "string";
  if (r && r.kind === "record") return isSingleTextObject(model, r) ? "string" : linkTo(recordAnchor(model, r), r.name);
  return "value";
}

/**
 * The valid string values of an enum-typed field (kebab-cased), or null.
 * @param {Model} model
 * @param {Object} type A DOM type node.
 * @returns {Array|null}
 */
function enumValues(model, type) {
  var t = type || {};
  if ((t.name || {}).identifier === "Optional") return enumValues(model, templateArg(t, 0));
  var r = recordFromType(model, t);
  if (!(r && r.kind === "enum")) return null;
  var cs = r.constants || [], out = [];
  for (var i = 0; i < cs.length; ++i) { var c = getSymbol(model, cs[i]); if (c && c.name) out.push(kebabCase(c.name)); }
  return out;
}

/**
 * The description cell for a field: its brief, the valid values when it is an
 * enum, and an "Optional." marker when the type is optional.
 * @param {Model} model
 * @param {Object} field A variable symbol.
 * @returns {string}
 */
function fieldDescription(model, field) {
  var t = field.type || {}, desc = briefText(field);
  var vals = enumValues(model, t);
  if (vals && vals.length) {
    var quoted = [];
    for (var i = 0; i < vals.length; ++i) quoted.push("`" + vals[i] + "`");
    desc += (desc ? " " : "") + "One of: " + quoted.join(", ") + ".";
  }
  if ((t.name || {}).identifier === "Optional") desc = desc ? ("Optional. " + desc) : "Optional.";
  return desc;
}

/**
 * The "| field | type | description" table for one record. Type cells are
 * monospaced.
 * @param {Model} model
 * @param {Object} rec A record symbol.
 * @returns {string}
 */
function fieldTable(model, rec) {
  var f = allFields(model, rec), s = "[cols=\"1,1,2\"]\n|===\n|Field |Type |Description\n\n";
  for (var i = 0; i < f.length; ++i) s += "|`" + toCamelCase(f[i].name) + "`\n|`" + typeCell(model, f[i].type) + "`\n|" + fieldDescription(model, f[i]) + "\n\n";
  return s + "|===\n\n";
}

/**
 * A reference section: an anchor, a heading, the type's brief paragraph, and
 * its field table.
 * @param {Model} model
 * @param {string} level The AsciiDoc heading level ("==" or "===").
 * @param {string} anchor The section anchor id.
 * @param {string} title The heading text.
 * @param {Object} rec The record documented by this section.
 * @returns {string}
 */
function sectionFor(model, level, anchor, title, rec) {
  var s = "[#" + anchor + "]\n" + level + " " + title + "\n\n";
  var brief = briefText(rec);
  if (brief) s += brief + "\n\n";
  return s + fieldTable(model, rec);
}

/**
 * The record whose name matches a variant base, for the group's brief.
 * @param {Model} model
 * @param {string} name A record name.
 * @returns {Object|null}
 */
function findRecordNamed(model, name) {
  for (var i = 0; i < model.records.length; ++i) if (model.records[i].name === name) return model.records[i];
  return null;
}

/**
 * Sort comparator by symbol name.
 * @param {Object} a
 * @param {Object} b
 * @returns {number}
 */
function byName(a, b) { return a.name < b.name ? -1 : 1; }

/**
 * Write the AsciiDoc reference partial: one section per variant group (with a
 * subsection per kind) followed by the remaining plain structures.
 * @param {Model} model
 * @returns {void}
 */
function emitAdoc(model) {
  var path = "reference/dom-schema.adoc", out = model.ctx.output;
  out.write(path,
    "// Generated by docs/mrdocs/extensions/schema.js (do not edit).\n" +
    "// Regenerate with the schema generator; CI checks it is up to date.\n\n" +
    "The fields below are reflected from MrDocs's metadata types. In each table, object and array types link to the section that documents them.\n\n");

  var covered = {};
  for (var gi = 0; gi < GROUPS.length; ++gi) {
    var base = GROUPS[gi][0], title = GROUPS[gi][1];
    var baseRec = findRecordNamed(model, base) || findRecordNamed(model, VARIANTS[base]);
    var groupBrief = baseRec ? briefText(baseRec) : "";
    out.append(path, "[#" + slugify(title) + "]\n== " + title + "\n\n" + (groupBrief ? groupBrief + "\n\n" : ""));
    var kinds = model.variantKinds[base].slice().sort(byName);
    for (var ki = 0; ki < kinds.length; ++ki) {
      covered[kinds[ki].id] = 1;
      out.append(path, sectionFor(model, "===", slugify(title) + "-" + kindTag(model, kinds[ki]), kindTag(model, kinds[ki]), kinds[ki]));
    }
  }

  var others = [];
  for (var oo = 0; oo < model.structOrder.length; ++oo) {
    var oid = model.structOrder[oo];
    if (!covered[oid]) { var orc = getSymbol(model, oid); if (orc) others.push(orc); }
  }
  others.sort(byName);
  if (others.length) {
    out.append(path, "[#other-structures]\n== Other Structures\n\n");
    for (var oi = 0; oi < others.length; ++oi) out.append(path, sectionFor(model, "===", slugify(others[oi].name), others[oi].name, others[oi]));
  }
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

mrdocs.register_generator("schema", function (ctx) {
  // Emit one artifact per run (`generator-options.schema.only` = rng|json|adoc,
  // empty = all), so each fits the small JS heap.
  var only = (ctx.params && ctx.params.only) || "";
  var model = buildModel(ctx);
  if (!only || only === "rng") emitRng(model);
  if (!only || only === "json") emitJson(model);
  if (!only || only === "adoc") emitAdoc(model);
});
