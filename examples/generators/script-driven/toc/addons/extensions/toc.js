mrdocs.register_generator("toc", function(ctx) {
  const roots = buildRoots(ctx.corpus);
  ctx.output.write("toc.md", markdown(roots, 0));
  ctx.output.write("toc.adoc", asciidoc(roots, 0));
  ctx.output.write("toc.html", html(roots));
});

/**
 * Build the top-level navigation nodes for the whole corpus: the page-level
 * children of the global namespace (the one symbol with no `parent`), each
 * expanded into a node tree.
 * @param {object} corpus - `ctx.corpus`.
 * @returns {object[]} The root nav nodes.
 */
function buildRoots(corpus) {
  let root = null;
  for (let i = 0; i < corpus.symbols.length; ++i) {
    if (!corpus.symbols[i].parent) {
      root = corpus.symbols[i];
      break;
    }
  }
  const roots = [];
  const topIds = root ? childIds(root) : [];
  for (let j = 0; j < topIds.length; ++j) {
    const top = corpus.get(topIds[j]);
    if (top && top.name) {
      roots.push(node(corpus, top));
    }
  }
  return roots;
}

// Page-level child categories of a namespace, in the order a nav should list
// them. Record members (methods, fields) live on the record's own page, so the
// walk descends through namespaces only and treats everything else as a leaf.
const CATEGORIES = [
  "namespaces", "records", "enums", "functions",
  "variables", "typedefs", "concepts", "usings", "macros"
];

/**
 * The ids of a namespace's page-level children, across every category in
 * `CATEGORIES`. Anything that is not a namespace is a leaf in the navigation
 * tree, so this returns an empty array for it.
 * @param {object} sym - The symbol whose children to collect.
 * @returns {string[]} The child symbol ids, in category order.
 */
function childIds(sym) {
  const ids = [];
  if (sym.kind !== "namespace" || !sym.members) {
    return ids;
  }
  for (let c = 0; c < CATEGORIES.length; ++c) {
    const list = sym.members[CATEGORIES[c]] || [];
    for (let k = 0; k < list.length; ++k) {
      ids.push(list[k]);
    }
  }
  return ids;
}

/**
 * Build a plain `{ name, url, children }` node for a symbol, recursing through
 * its page-level children. Only namespaces have children; every other symbol
 * is a leaf linking to its own page. Unnamed children are skipped.
 * @param {object} corpus - `ctx.corpus`.
 * @param {object} sym - The symbol to convert.
 * @returns {{name: string, url: string, children: object[]}} The nav node.
 */
function node(corpus, sym) {
  const children = [];
  const ids = childIds(sym);
  for (let i = 0; i < ids.length; ++i) {
    const child = corpus.get(ids[i]);
    if (child && child.name) {
      children.push(node(corpus, child));
    }
  }
  return { name: sym.name, url: urlFor(corpus, sym), children: children };
}

/**
 * The page URL for a symbol: the chain of anchors from the global namespace
 * down to the symbol, joined by "/", plus ".html" (the shape the HTML
 * generator emits).
 * @param {object} corpus - `ctx.corpus`, used to resolve each `parent` id.
 * @param {object} sym - The symbol to build a URL for.
 * @returns {string} The symbol's page URL.
 */
function urlFor(corpus, sym) {
  const parts = [];
  let cur = sym;
  while (cur && cur.parent) {
    parts.unshift(cur.anchor);
    cur = corpus.get(cur.parent);
  }
  return parts.join("/") + ".html";
}

/**
 * Render nav nodes as a nested Markdown bullet list.
 * @param {object[]} nodes - Nav nodes.
 * @param {number} depth - Indentation depth, 0 at the top level.
 * @returns {string} The Markdown list.
 */
function markdown(nodes, depth) {
  let out = "";
  for (let i = 0; i < nodes.length; ++i) {
    const n = nodes[i];
    out += "  ".repeat(depth) + "- [" + n.name + "](" + n.url + ")\n";
    out += markdown(n.children, depth + 1);
  }
  return out;
}

/**
 * Render nav nodes as a nested AsciiDoc list of `xref` links.
 * @param {object[]} nodes - Nav nodes.
 * @param {number} depth - Nesting depth, 0 at the top level (one `*` per level).
 * @returns {string} The AsciiDoc list.
 */
function asciidoc(nodes, depth) {
  let out = "";
  for (let i = 0; i < nodes.length; ++i) {
    const n = nodes[i];
    out += "*".repeat(depth + 1) + " xref:" + n.url + "[" + n.name + "]\n";
    out += asciidoc(n.children, depth + 1);
  }
  return out;
}

/**
 * Render nav nodes as a nested HTML `<ul>` list. Returns an empty string for
 * an empty list, so leaf nodes emit no child `<ul>`.
 * @param {object[]} nodes - Nav nodes.
 * @returns {string} The HTML list.
 */
function html(nodes) {
  if (nodes.length === 0) {
    return "";
  }
  let out = "<ul>\n";
  for (let i = 0; i < nodes.length; ++i) {
    const n = nodes[i];
    out += "<li><a href=\"" + n.url + "\">" + n.name + "</a>" + html(n.children) + "</li>\n";
  }
  out += "</ul>\n";
  return out;
}
