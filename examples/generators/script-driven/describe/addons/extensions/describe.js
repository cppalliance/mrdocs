mrdocs.register_generator("describe", function(ctx) {
  const grouped = groupByHeader(ctx.corpus);
  for (let i = 0; i < grouped.order.length; ++i) {
    const header = grouped.order[i];
    const group = grouped.byHeader[header];
    ctx.output.write(sidecarName(header), renderSidecar(header, group.includes, group.order, group.byNs));
  }
});

/**
 * Group the describe annotation of every record and enum in the corpus, first
 * by the header it is defined in and then by its enclosing namespace, keeping
 * first-seen order at every level. Each header group also collects the base
 * sidecars its types depend on, minus its own.
 * @param {object} corpus - `ctx.corpus`.
 * @returns {{order: string[], byHeader: Object<string, {order: string[], byNs: Object<string, string[]>, includes: string[]}>}}
 *   The headers in first-seen order and, for each, its namespaces, their
 *   annotation lines, and the base sidecars to include.
 */
function groupByHeader(corpus) {
  const order = [];
  const byHeader = {};
  for (let i = 0; i < corpus.symbols.length; ++i) {
    const sym = corpus.symbols[i];
    const line = describeMacro(corpus, sym);
    if (line === null) {
      continue;
    }
    const header = sourceHeaderOf(sym);
    if (!header) {
      continue;
    }
    if (!(header in byHeader)) {
      byHeader[header] = { order: [], byNs: {}, includes: [] };
      order.push(header);
    }
    const group = byHeader[header];
    const ns = namespaceOf(corpus, sym);
    if (!(ns in group.byNs)) {
      group.byNs[ns] = [];
      group.order.push(ns);
    }
    group.byNs[ns].push(line);

    const self = sidecarName(header);
    const bases = baseSidecars(corpus, sym);
    for (let s = 0; s < bases.length; ++s) {
      if (bases[s] !== self && group.includes.indexOf(bases[s]) === -1) {
        group.includes.push(bases[s]);
      }
    }
  }
  return { order: order, byHeader: byHeader };
}

/**
 * The Boost.Describe annotation for a symbol: BOOST_DESCRIBE_STRUCT for a
 * record (its base classes and own public members) or BOOST_DESCRIBE_ENUM for
 * an enum (its enumerators). Returns null for any other kind of symbol.
 * @param {object} corpus - `ctx.corpus`.
 * @param {object} sym - The symbol to annotate.
 * @returns {string|null} The macro invocation, or null if `sym` is not a
 *   record or enum.
 */
function describeMacro(corpus, sym) {
  if (sym.kind === "record") {
    const bases = [];
    const baseList = sym.bases || [];
    for (let b = 0; b < baseList.length; ++b) {
      const name = baseList[b].type && baseList[b].type.name && baseList[b].type.name.identifier;
      if (name) {
        bases.push(name);
      }
    }
    const members = ownMemberNames(corpus, sym, "variables").concat(ownMemberNames(corpus, sym, "functions"));
    return "BOOST_DESCRIBE_STRUCT(" + sym.name + ", (" + bases.join(", ") + "), (" + members.join(", ") + "))";
  }
  if (sym.kind === "enum") {
    const values = [];
    const constants = sym.constants || [];
    for (let c = 0; c < constants.length; ++c) {
      const value = corpus.get(constants[c]);
      if (value && value.name) {
        values.push(value.name);
      }
    }
    return "BOOST_DESCRIBE_ENUM(" + sym.name + ", " + values.join(", ") + ")";
  }
  return null;
}

/**
 * The names of a record's own public members in `category`. The interface also
 * carries inherited members; Boost.Describe learns those from the base list, so
 * only members declared on this record (their `parent` is the record) are kept.
 * @param {object} corpus - `ctx.corpus`.
 * @param {object} record - The record symbol.
 * @param {string} category - The interface bucket, "variables" or "functions".
 * @returns {string[]} The own public member names, in declaration order.
 */
function ownMemberNames(corpus, record, category) {
  const names = [];
  const list = record.interface.public[category] || [];
  for (let i = 0; i < list.length; ++i) {
    const member = corpus.get(list[i]);
    if (member && member.name && member.parent === record.id) {
      names.push(member.name);
    }
  }
  return names;
}

/**
 * The fully-qualified enclosing namespace of a symbol ("a::b"), or "" when it
 * sits at global scope. A BOOST_DESCRIBE_STRUCT must appear in the type's own
 * namespace so the unqualified name resolves, so entries are grouped by this.
 * @param {object} corpus - `ctx.corpus`, used to walk `parent` ids.
 * @param {object} sym - The symbol whose namespace to compute.
 * @returns {string} The enclosing namespace, "::"-joined, or "" at global scope.
 */
function namespaceOf(corpus, sym) {
  const parts = [];
  let cur = sym.parent ? corpus.get(sym.parent) : null;
  while (cur && cur.name !== undefined && cur.kind === "namespace") {
    parts.unshift(cur.name);
    cur = cur.parent ? corpus.get(cur.parent) : null;
  }
  return parts.join("::");
}

/**
 * The header a symbol is defined in, taken from its definition location, or ""
 * when the location is unknown. Sidecars are grouped by this so each one can
 * include exactly the header whose types it annotates.
 * @param {object} sym - The symbol whose defining header to read.
 * @returns {string} The header path, or "" if unknown.
 */
function sourceHeaderOf(sym) {
  return (sym.loc && sym.loc.defLoc && sym.loc.defLoc.shortPath) || "";
}

/**
 * The sidecar file name for a header: "foo.hpp" becomes "foo.described.hpp", and
 * a header with any other (or no) extension gets ".described.hpp" appended.
 * Including it gives the described version of that header's types.
 * @param {string} header - The source header path.
 * @returns {string} The sidecar path.
 */
function sidecarName(header) {
  const dot = header.lastIndexOf(".");
  const slash = header.lastIndexOf("/");
  const stem = dot > slash ? header.slice(0, dot) : header;
  return stem + ".described.hpp";
}

/**
 * The sidecars of a record's described base classes: for each base that
 * resolves to a record or enum this generator also describes, the sidecar of
 * the header that base is defined in. Reflecting the record's inherited members
 * needs those base descriptors present.
 * @param {object} corpus - `ctx.corpus`.
 * @param {object} sym - The record whose bases to resolve.
 * @returns {string[]} The base sidecar paths, in base order.
 */
function baseSidecars(corpus, sym) {
  const sidecars = [];
  const baseList = sym.bases || [];
  for (let b = 0; b < baseList.length; ++b) {
    const id = baseList[b].type && baseList[b].type.name && baseList[b].type.name.id;
    const base = id ? corpus.get(id) : null;
    if (base && describeMacro(corpus, base) !== null) {
      const header = sourceHeaderOf(base);
      if (header) {
        sidecars.push(sidecarName(header));
      }
    }
  }
  return sidecars;
}

/**
 * Assemble one sidecar header: an include guard, a do-not-edit banner, an
 * include of the source header and of any base sidecars, the Boost.Describe
 * include, and each namespace's annotations wrapped in a `namespace { ... }`
 * block (a global-scope group, keyed by "", is emitted unwrapped).
 * @param {string} header - The source header this sidecar annotates.
 * @param {string[]} includes - Base sidecar paths to include first.
 * @param {string[]} nsOrder - Namespaces in emission order.
 * @param {Object<string, string[]>} byNs - Annotation lines per namespace.
 * @returns {string} The sidecar header text.
 */
function renderSidecar(header, includes, nsOrder, byNs) {
  const guard = guardName(sidecarName(header));
  let out =
    "#ifndef " + guard + "\n" +
    "#define " + guard + "\n\n" +
    "// Generated by MrDocs from documented types. Do not edit by hand.\n" +
    "//\n" +
    "// Include this to gain Boost.Describe reflection over the types declared\n" +
    "// in " + header + ".\n\n" +
    "#include \"" + header + "\"\n";
  for (let i = 0; i < includes.length; ++i) {
    out += "#include \"" + includes[i] + "\"\n";
  }
  out += "#include <boost/describe.hpp>\n\n";
  for (let g = 0; g < nsOrder.length; ++g) {
    const ns = nsOrder[g];
    if (ns) {
      out += "namespace " + ns + " {\n\n";
    }
    out += byNs[ns].join("\n") + "\n";
    if (ns) {
      out += "\n}  // namespace " + ns + "\n";
    }
    out += "\n";
  }
  out += "#endif  // " + guard + "\n";
  return out;
}

/**
 * An include-guard macro for a sidecar path: uppercased, every run of
 * non-alphanumeric characters collapsed to a single "_", with a leading "_"
 * added if the result would start with a digit.
 * @param {string} sidecar - The sidecar path.
 * @returns {string} The guard macro name.
 */
function guardName(sidecar) {
  let guard = sidecar.toUpperCase().replace(/[^A-Z0-9]+/g, "_");
  if (/^[0-9]/.test(guard)) {
    guard = "_" + guard;
  }
  return guard;
}
