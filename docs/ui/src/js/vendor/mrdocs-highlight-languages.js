'use strict'

//
// Shared highlight.js language definitions for Mr.Docs.
//
// The doc site (this UI bundle) and the marketing landing page
// (docs/website/render.js) both need the exact same C++ colouring: the
// doc-block comments render green with their `@tag` keywords, parameter
// names, and backtick code spans in a lighter green. That behaviour lives
// in `richCpp` below (plus `fixCppLanguage`), and it is genuinely fiddly,
// so it is defined once here and consumed from both places instead of being
// copied into each.
//
// This module is environment-agnostic: it never touches the DOM and does
// not run any highlighting itself. `register(hljs)` installs the languages
// on a highlight.js instance the caller already has; `create()` builds a
// fresh instance and registers them. The browser bundle
// (`highlight.bundle.js`) calls `register` and then walks the DOM; the
// Node renderer calls `create` and highlights strings.
//
// Everything targets the highlight.js 9.x API (the version the UI bundle is
// built against), so both consumers must resolve highlight.js 9.x. The
// language module `require`s below resolve from this file's location
// (docs/ui/node_modules), which pins that version for both callers.
//

// The default bash language only paints keywords, builtins, strings and
// variables. Command-line invocations such as
//   python bootstrap.py --yes --build-type Release
// contain none of those, so they would otherwise render as flat text.
// We wrap the upstream definition and add two more rules: the first
// word of each line is the command being run, and `-x` / `--xy` tokens
// are option flags. Both stay at relevance 0 so they do not pull
// language detection away from real bash scripts.
function commandAwareBash (hl) {
  var def = require('highlight.js/lib/languages/bash')(hl)
  def.contains = (def.contains || []).slice()
  // Match GNU-style long flags and POSIX short flags only when the
  // hyphen sits at the start of a token: preceded by start-of-line
  // or whitespace, never inside a longer identifier. Without the
  // lookbehind, `-party` in `../third-party` would be picked up as
  // a flag and rendered in flag color, splitting the path.
  def.contains.unshift({
    className: 'attr',
    begin: /(?<=^|\s)--?[A-Za-z][A-Za-z0-9-]*/,
    relevance: 0,
  })
  // The first identifier on a line is the command being run. Skip
  // continuation lines (those starting with whitespace + `-`, which
  // are continued flag lists) by requiring the first non-space
  // character to be a letter, digit, slash, or dot — not a hyphen.
  def.contains.unshift({
    className: 'title',
    begin: /^[ \t]*[A-Za-z0-9_./][A-Za-z0-9_./-]*/,
    relevance: 0,
  })
  return def
}
// Mr.Docs's own subject matter is documentation, so the snippets we
// show contain heavy Javadoc/Doxygen comments. By default highlight.js
// paints the whole comment in one muted gray, which makes the most
// informative part of the page the hardest to read. Wrap the cpp
// language so doc-block comments (those starting with `/**` or
// `/*!`) carry their own contained tokens: `@param`, `@return`,
// backtick code spans, and so on stand out against the prose.
// Fixes for highlight.js's upstream cpp language. Two of its
// heuristics paint C++ tokens with the wrong meaning and the result
// reads as random color rather than syntax:
//
//  - CPP_PRIMITIVE_TYPES matches any identifier ending in `_t` as a
//    keyword. So `enable_if_t` gets keyword color while the next
//    line's `is_integral_v` (same kind of type alias from
//    <type_traits>, just no `_t` suffix) gets no class at all. The
//    asymmetry is the visible bug, but the deeper problem is that
//    user-defined `*_t` names — `error_t`, `result_t`, anything in
//    your own code — are *not* keywords.
//  - The built-in list hard-codes `std`. The standard namespace is
//    not a built-in; treating it as one means `std::foo` renders
//    with `std` accented and `foo` plain, which is the opposite of
//    the reading the eye does (everything after `::` is the
//    interesting part).
//
// Strip both miscategorisations so type-alias names and namespace
// qualifiers all fall through to the plain foreground color. That
// keeps the right tokens (`int`, `template`, `requires`, `noexcept`)
// accented and lets the asymmetric pairs read as one block.
// Patches the upstream highlight.js cpp language so token output
// matches what a C++-aware reader expects. All changes are generic:
// they apply to any C++ source, not to a hardcoded stdlib list.
//
//  1. CPP_PRIMITIVE_TYPES (a regex tagging any `_t`-suffixed identifier
//     as a keyword) is removed. `enable_if_t`, `error_t`, `result_t`,
//     any user type ending in `_t` — none of these are keywords.
//  2. `std` is removed from `keywords.built_in`. It is a namespace,
//     not a built-in; the new qualified-name rules below pick it up
//     uniformly with every other namespace.
//  3. Two qualified-name rules added: the identifier immediately
//     before `::`, and the identifier immediately after `::`. Both
//     are tagged `type`, so `std::enable_if_t`, `boost::asio::ip`,
//     `MyNamespace::Inner::leaf` all render symmetrically. No
//     stdlib-specific behavior.
//  4. An attribute rule for `[[ ... ]]` so `[[nodiscard]]`,
//     `[[noreturn]]`, and user attributes stand out as meta.
//  5. The injected rules are added to every nested `contains` array
//     so they fire inside the FUNCTION_DECLARATION mode and inside
//     the `params` paren-list mode — the two places hljs upstream
//     would otherwise leave qualified names unclassified.
function fixCppLanguage (def) {
  function isWildcardTypeMode (mode) {
    return mode &&
      mode.className === 'keyword' &&
      typeof mode.begin === 'string' &&
      mode.begin === '\\b[a-z\\d_]*_t\\b'
  }
  // The upstream `class` mode fires on the keyword `class` or `struct`
  // anywhere, and ends only at `{`, `;`, or `:`. Inside a template
  // parameter list — `template <class T> void bar();` — there is no
  // `{` after `T`, so the class mode swallows `T> void bar()` all the
  // way to the trailing semicolon, tagging `T`, `void`, `bar`, and
  // anything else inside as fake `title`s. Replace the begin pattern
  // with one that demands the class name be followed by `{` or `:`,
  // so it only matches real class/struct definitions.
  function isClassMode (mode) {
    return mode &&
      mode.className === 'class' &&
      typeof mode.beginKeywords === 'string' &&
      /\bclass\b/.test(mode.beginKeywords) &&
      /\bstruct\b/.test(mode.beginKeywords)
  }
  function patch (rules) {
    if (!Array.isArray(rules)) return rules
    return rules.filter(function (r) {
      return !isWildcardTypeMode(r)
    }).map(function (r) {
      if (isClassMode(r)) {
        r = Object.assign({}, r)
        delete r.beginKeywords
        r.begin = /\b(?:class|struct|union)\s+[A-Za-z_]\w*(?=\s*[:{])/
        // Put the matched text back so the inner contains can
        // tokenise it. Without this the class wrapper opens but
        // the class name renders plain because the begin regex
        // already consumed it.
        r.returnBegin = true
        // hljs.TITLE_MODE matches any identifier — including
        // `class`/`struct`/`union` themselves, which would
        // otherwise get the function-name `title` color
        // alongside the actual class name. Tag those keywords
        // explicitly first so they stay red and only the class
        // name gets the title color.
        var classContains = (r.contains || []).slice()
        classContains.unshift({
          className: 'keyword',
          begin: /\b(?:class|struct|union)\b/,
          relevance: 0,
        })
        r.contains = classContains
      }
      if (r && Array.isArray(r.contains)) {
        r = Object.assign({}, r, { contains: patch(r.contains) })
      }
      return r
    })
  }
  def.contains = patch(def.contains)

  // Clear the entire built-in list. Upstream cpp.js hard-codes
  // stdlib names (`vector`, `set`, `map`, `string`, `sqrt`,
  // `printf`, the stream objects, math functions, etc.) as
  // built-ins, so an unqualified `sqrt(x)` renders orange while a
  // user's own `sqrt(x)` of the same shape would render plain.
  // That contradicts the principle that stdlib names should get
  // no treatment beyond what any other identifier does. The new
  // qualified-name rules above still highlight `std::sqrt`,
  // `std::vector<int>`, etc. through the namespace/qualified-part
  // tags — those fire on any `Name::Name`, stdlib or not.
  if (def.keywords && typeof def.keywords.built_in === 'string') {
    def.keywords.built_in = ''
  }

  // Generic qualified-name rules. `\b[A-Za-z_]\w*(?=::)` catches the
  // namespace/class name before any `::`; `(?<=::)[A-Za-z_]\w*` catches
  // the identifier after. Both use `type` className so the two halves
  // of every qualified name render symmetrically, regardless of
  // whether the namespace is `std`, `boost`, or a user library.
  var NAMESPACE_PART = {
    className: 'type',
    begin: /\b[A-Za-z_]\w*(?=::)/,
    relevance: 0,
  }
  var QUALIFIED_PART = {
    className: 'type',
    begin: /(?<=::)[A-Za-z_]\w*/,
    relevance: 0,
  }
  // Attributes like `[[nodiscard]]` and `[[deprecated("reason")]]`.
  // Marked `meta` so they get the same blue tint as preprocessor
  // directives.
  var ATTRIBUTE_RULE = {
    className: 'meta',
    begin: /\[\[/,
    end: /\]\]/,
    relevance: 0,
  }
  // The upstream function-declaration mode's title sub-rule matches
  // any `IDENT(` pattern and tags the identifier as `title`. Inside
  // a function declaration this fires multiple times — for `noexcept(`,
  // `requires(`, `explicit(`, etc. — and tags those keywords as
  // titles (purple, like a function name). Pre-match those tokens as
  // keywords before the title rule gets a chance, so `noexcept(B)`,
  // `requires(...)`, `alignas(...)`, etc. stay red.
  var KEYWORD_WITH_PAREN = {
    className: 'keyword',
    begin: /\b(?:noexcept|requires|explicit|alignas|alignof|decltype|sizeof|typeid|throw|return|if|while|for|switch|catch|co_await|co_yield|co_return|static_assert)\b(?=\s*\()/,
    relevance: 0,
  }
  // Type-name heuristic for variable declarations: `TYPE name = ...`,
  // `TYPE name;`, `TYPE name(...)`, `TYPE name{...}`. The
  // identifier before another identifier + `=`/`;`/`{`/`,` is the
  // type. This catches `sqrt_fn sqrt = {};`, `MyClass obj;`, etc.,
  // including user-defined types that aren't in any keyword list.
  // The negative lookahead excludes keywords that themselves can
  // sit before an identifier (`return value`, `throw err`,
  // `delete x`, etc.) so they don't get misclassified as types.
  var KW_LIST = (
    'return throw delete new auto else case default do goto break continue ' +
    'sizeof alignof alignas noexcept requires explicit typename class struct ' +
    'union enum template namespace using typedef virtual override final ' +
    'static extern register mutable volatile const constexpr consteval ' +
    'constinit inline thread_local public private protected friend operator ' +
    'static_cast const_cast dynamic_cast reinterpret_cast ' +
    'true false nullptr this if while for switch catch try ' +
    'int float double char bool void short long signed unsigned ' +
    'wchar_t char8_t char16_t char32_t asm import module export decltype'
  ).split(/\s+/).join('|')
  var TYPE_IN_DECL = {
    className: 'type',
    begin: new RegExp(
      '\\b(?!(?:' + KW_LIST + ')\\b)' +
      '[A-Za-z_]\\w*' +
      '(?=\\s+[A-Za-z_]\\w*\\s*[=;{,])'
    ),
    relevance: 0,
  }
  var INJECTED = [KEYWORD_WITH_PAREN, NAMESPACE_PART, QUALIFIED_PART, TYPE_IN_DECL, ATTRIBUTE_RULE]

  // Walk the grammar tree and prepend the injected rules into every
  // `contains` array. Highlight.js scopes contains per-mode, so unless
  // these rules live in the params mode and the function-declaration
  // mode (as well as top level), they would never fire inside a
  // function signature — the exact place qualified return types show
  // up. The WeakSet guards against revisiting the same array twice
  // when upstream cpp.js wires two modes to one shared `contains`.
  //
  // The opaque-mode check stops recursion into comment and string
  // modes. Comments have their own contains (for inline `@TODO`,
  // doctag, etc.) and so do strings (for escape sequences); if we
  // inject our code-aware rules into those, `TYPE_IN_DECL` will fire
  // on prose like `integral value` and paint `integral` orange.
  function isOpaque (r) {
    var c = r && r.className
    return c === 'comment' || c === 'doc-comment' ||
      c === 'string' || c === 'regexp' || c === 'doctag' ||
      c === 'meta-string'
  }
  function inject (rules, seen) {
    if (!Array.isArray(rules)) return
    if (seen.has(rules)) return
    seen.add(rules)
    rules.unshift.apply(rules, INJECTED)
    for (var i = 0; i < rules.length; i++) {
      var r = rules[i]
      if (r && Array.isArray(r.contains) && !isOpaque(r)) inject(r.contains, seen)
    }
  }
  inject(def.contains, new WeakSet())
  return def
}

// Build the doc-comment-aware cpp language on top of an injected base cpp
// language definition. Injecting the base (instead of requiring highlight.js
// here) lets a Node caller such as docs/website/render.js supply it from its
// own highlight.js install, so this module never has to resolve highlight.js
// from docs/ui at runtime. The browser bundle and register() pass the base in
// from the same highlight.js they already load.
function makeRichCpp (cppLangDef) {
  return function (hl) { return richCppImpl(hl, cppLangDef) }
}

function richCppImpl (hl, cppLangDef) {
  var def = fixCppLanguage(cppLangDef(hl))
  // Doc-comment children carry their own classes so `@param`, the
  // parameter name after it, and inline `code` spans inside the
  // prose each pick up their own color against the comment body.
  var docCommentChildren = [
    {
      className: 'doctag',
      begin: /@\w+/,
      relevance: 0,
    },
    {
      className: 'name',
      begin: /(?<=@(?:param|tparam|throws|exception)\b\s+)[A-Za-z_]\w*/,
      relevance: 0,
    },
    {
      className: 'string',
      begin: /`[^`]+`/,
      relevance: 0,
    },
  ]
  // Doxygen-style block doc comments: `/** ... */` and `/*! ... */`.
  var docBlockComment = {
    className: 'doc-comment',
    begin: /\/\*[*!]/,
    end: /\*\//,
    contains: docCommentChildren,
  }
  // Doxygen-style line doc comments: `///`, `//!`, `///<`, `//!<`.
  // The lookahead on `///` blocks plain `////` separators from
  // matching as a (zero-content) doc comment.
  var docLineComment = {
    className: 'doc-comment',
    begin: /\/\/[!/](?!\/)<?/,
    end: /$/,
    contains: docCommentChildren,
  }
  // Identify the C block-comment mode by structure rather than by
  // reference. `fixCppLanguage` clones mode objects when it
  // recurses into their contains, so `hl.C_BLOCK_COMMENT_MODE` and
  // the cloned copy are no longer the same reference; comparing by
  // `begin` pattern catches both.
  function isCBlockComment (r) {
    if (!r) return false
    var b = r.begin
    if (typeof b === 'string') return b === '/\\*'
    if (b instanceof RegExp) return b.source === '/\\*'
    return false
  }
  function isCLineComment (r) {
    if (!r) return false
    var b = r.begin
    if (typeof b === 'string') return b === '//'
    if (b instanceof RegExp) return b.source === '//'
    return false
  }
  function rewrite (rules) {
    if (!rules) return rules
    // Insert the block/line doc-comment modes ahead of the plain
    // block/line comment modes at every level so the doc forms are
    // matched first.
    var out = rules.slice()
    for (var i = 0; i < out.length; i++) {
      if (isCBlockComment(out[i])) {
        out.splice(i, 0, docBlockComment)
        ++i
      } else if (isCLineComment(out[i])) {
        out.splice(i, 0, docLineComment)
        ++i
      }
    }
    return out.map(function (r) {
      if (r && Array.isArray(r.contains) &&
          r !== docBlockComment && r !== docLineComment) {
        r = Object.assign({}, r, { contains: rewrite(r.contains) })
      }
      return r
    })
  }
  def.contains = rewrite(def.contains)
  return def
}
// Build a Handlebars language definition layered on top of a given
// host language. The upstream `handlebars.js` hard-codes `xml` as
// the sub-language; that is correct for an `.html.hbs` partial but
// wrong for an `.adoc.hbs` one where the surrounding text should
// pick up AsciiDoc colors. The factory produces two definitions
// from the same mustache rule set: one with `xml` as host (the
// default `handlebars` tag, for HTML templates) and one with
// `asciidoc` (`adoc-handlebars`, for AsciiDoc templates).
function handlebarsOver (hostLang) {
  return function (hl) {
    var def = require('highlight.js/lib/languages/handlebars')(hl)
    def.subLanguage = hostLang
    // Walk the mustache rules and replace any nested subLanguage
    // (the raw-block `{{{{raw}}}}` opener carries one) so a script
    // that nests an asciidoc literal inside `{{{{raw}}}}...{{{{/raw}}}}`
    // still renders the inner content with the host language.
    ;(def.contains || []).forEach(function (rule) {
      if (rule && rule.starts && rule.starts.subLanguage) {
        rule.starts.subLanguage = hostLang
      }
    })
    // Drop the upstream aliases so the two flavours stay distinct.
    def.aliases = []
    return def
  }
}

// Register every language the docs and landing page use on the given
// highlight.js instance.
function register (hljs) {
  // Only the languages our docs actually use. `grep -rohE
  // '\[source,[a-zA-Z+#-]+\]' docs/modules/ROOT/pages` yields:
  // bash, c++ (alias of cpp), cmake, handlebars, ini, javascript,
  // json, latex, lua, markdown, powershell, text/txt, xml, yaml.
  // Anything else falls back to unstyled.
  var richCpp = makeRichCpp(require('highlight.js/lib/languages/cpp'))
  hljs.registerLanguage('bash', commandAwareBash)
  hljs.registerLanguage('cmake', require('highlight.js/lib/languages/cmake'))
  hljs.registerLanguage('cpp', richCpp)
  hljs.registerLanguage('ini', require('highlight.js/lib/languages/ini'))
  hljs.registerLanguage('javascript', require('highlight.js/lib/languages/javascript'))
  hljs.registerLanguage('json', require('highlight.js/lib/languages/json'))
  hljs.registerLanguage('latex', require('highlight.js/lib/languages/tex'))
  hljs.registerLanguage('lua', require('highlight.js/lib/languages/lua'))
  hljs.registerLanguage('markdown', require('highlight.js/lib/languages/markdown'))
  hljs.registerLanguage('plaintext', require('highlight.js/lib/languages/plaintext'))
  hljs.registerLanguage('powershell', require('highlight.js/lib/languages/powershell'))
  hljs.registerLanguage('xml', require('highlight.js/lib/languages/xml'))
  hljs.registerLanguage('yaml', require('highlight.js/lib/languages/yaml'))
  // AsciiDoc is the host language for adoc-handlebars and is also
  // useful on its own for the few `[source,asciidoc]` blocks the
  // docs already use.
  hljs.registerLanguage('asciidoc', require('highlight.js/lib/languages/asciidoc'))
  // Handlebars templates: `handlebars` (and `hbs`/`html-handlebars`)
  // for HTML-flavoured templates such as `code-block.html.hbs`,
  // `adoc-handlebars` for AsciiDoc-flavoured templates such as
  // `wrapper.adoc.hbs`. Both share the same mustache colouring but
  // tokenise their surroundings as HTML or AsciiDoc respectively.
  hljs.registerLanguage('handlebars', handlebarsOver('xml'))
  hljs.registerLanguage('hbs', handlebarsOver('xml'))
  hljs.registerLanguage('html-handlebars', handlebarsOver('xml'))
  hljs.registerLanguage('adoc-handlebars', handlebarsOver('asciidoc'))
  // Aliases for the language tags the docs happen to use.
  // highlight.js 9.18.3 has no `registerAliases` API, so each alias
  // is a fresh `registerLanguage` call against the same definition.
  var plainDef = require('highlight.js/lib/languages/plaintext')
  hljs.registerLanguage('c++', richCpp)
  hljs.registerLanguage('adoc', require('highlight.js/lib/languages/asciidoc'))
  hljs.registerLanguage('text', plainDef)
  hljs.registerLanguage('txt', plainDef)
  hljs.registerLanguage('none', plainDef)
  return hljs
}

// Build a fresh highlight.js 9.x instance with the languages registered.
// Callers that already hold an instance (the browser bundle) should use
// `register` instead.
function create () {
  return register(require('highlight.js/lib/highlight'))
}

module.exports = { register: register, create: create, makeRichCpp: makeRichCpp }
