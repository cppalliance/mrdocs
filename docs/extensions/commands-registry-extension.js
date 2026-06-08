'use strict'

//
// Generate every command's partial from the registry at build time.
//
// Each component-version brings its own
// `partials/commands-registry.json`. This extension finds that file
// in the content catalog per version and synthesizes virtual partials:
//
//   * one per entry under `partials/commands/<slug>.adoc` (heading,
//     kind badge, syntaxes line, prose description, example listing,
//     the `.adoc-preview` block for non-Planned entries, see-also,
//     and status), for granular includes; and
//   * one aggregate per reference category under
//     `partials/commands/groups/<group>.adoc`, concatenating its
//     commands in registry order.
//
// No `.adoc` file is on disk. The reference page includes the three
// group partials, so adding a command to the registry surfaces it on
// the page with no further edits.
//
// Versions whose source tree does not carry the registry file are
// skipped: an older branch with a different commands page keeps its
// original rendering. The extension never reads from the local
// working tree, so develop's registry never bleeds into a tag build.
//
// Syntax tokens in the registry are stored bare. `formatSyntaxToken`
// turns each into AsciiDoc: backticks for inline code, `pass:[]` when
// AsciiDoc-special characters need to stay literal, `pass:c[]` for
// HTML so the angle brackets render as text, ` / ` splitting for
// paired commands like `@code / @endcode`, and trailing parenthesised
// annotations kept bare alongside the formatted syntax.
//

// The reference page is organized into top-level sections. Each maps to
// one or more registry sub-keys; `subs: null` means every sub-key under
// the top-level section (used for the inline groups, which all collapse
// into a single "Inlines" section). `title` is the section heading the
// group partial emits, so the reference page is just a list of includes.
const GROUPS = [
  { title: 'Markup Blocks', top: 'block_commands', subs: ['markup_blocks'] },
  { title: 'Metadata Blocks', top: 'block_commands', subs: ['metadata_blocks'] },
  { title: 'Inlines', top: 'inline_commands', subs: null },
]

function indexRegistry (data) {
  const entries = {}
  for (const topKey of ['block_commands', 'inline_commands']) {
    const section = data[topKey] || {}
    for (const subKey of Object.keys(section)) {
      for (const entry of section[subKey] || []) {
        const match = /^commands\/(.+)\.adoc$/.exec(entry.partial || '')
        if (match) entries[match[1]] = entry
      }
    }
  }
  return entries
}

function formatSyntaxToken (token) {
  if (token === 'not yet determined') return token
  if (/^\(.+\)$/.test(token)) return token.slice(1, -1)
  if (token.includes(' / ')) {
    return token.split(' / ').map(formatSyntaxToken).join(' / ')
  }
  const ann = /^(.+?)\s+(\([^()]+\))$/.exec(token)
  if (ann) return `${formatSyntaxToken(ann[1])} ${ann[2]}`
  // Monospace literal (backtick-plus): renders the token verbatim with no
  // AsciiDoc substitutions, so placeholders like `<name>`, `[<dir>]`, and
  // `{ description }` survive (the `]` would break a `pass:c[...]` macro).
  if (/[<>&*_=~^${}\[\]`]/.test(token)) return `\`+${token}+\``
  return `\`${token}\``
}

function exampleSlug (entry) {
  const ex = (entry.examples || [])[0] || ''
  return ex.replace(/^commands\//, '')
}

// Stable anchor for a command, derived from its heading.
function cmdAnchor (heading) {
  return 'cmd-' + heading.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '')
}

// AsciiDoc xref for a parameter type. `Paragraph` is a documentation node
// with its own command, so it links there; the rest are explained in the
// reference page's Parameter Types section.
function typeLink (type) {
  if (type === 'Paragraph') return `<<${cmdAnchor('Paragraph')},Paragraph>>`
  return `<<ptype-${type.toLowerCase()},${type}>>`
}

function renderPartial (entry) {
  const lines = [
    `[#${cmdAnchor(entry.heading)}]`,
    `=== ${entry.heading}`,
    '',
  ]
  const tokens = (entry.syntaxes || []).map(formatSyntaxToken)
  if (tokens.length === 1) {
    lines.push(`*Syntax:* ${tokens[0]}`)
    lines.push('')
  } else if (tokens.length > 1) {
    lines.push('*Syntaxes:*')
    lines.push('')
    for (const t of tokens) lines.push(`* ${t}`)
    lines.push('')
  }
  if (entry.description) {
    lines.push(entry.description)
    lines.push('')
  }
  if (entry.note) {
    lines.push('[NOTE]')
    lines.push('====')
    lines.push(entry.note)
    lines.push('====')
    lines.push('')
  }
  if (entry.parameters && entry.parameters.length) {
    lines.push('[cols="1,1,3"]')
    lines.push('|===')
    lines.push('| Type | Parameter | Description')
    lines.push('')
    for (const p of entry.parameters) {
      lines.push(`| ${typeLink(p.type)}`)
      lines.push(`| ${formatSyntaxToken(p.name)}`)
      lines.push(`| ${p.description}`)
    }
    lines.push('|===')
    lines.push('')
  }
  if (entry.planned) {
    // Recognized as a node in the AST, but the parser does not produce
    // it yet. Make that explicit instead of implying a missing example.
    lines.push('[NOTE]')
    lines.push('====')
    lines.push('Not yet supported: MrDocs does not yet produce correct output for this command.')
    lines.push('====')
    lines.push('')
  } else if (entry.examples && entry.examples.length) {
    const slug = exampleSlug(entry)
    // Source followed by its rendered output. The eight-`=` adoc-preview
    // wrapper survives any four-`=` admonitions in the rendered output.
    lines.push('.Example')
    lines.push('[source,cpp]')
    lines.push('----')
    lines.push(`include::example$snippets/commands/${slug}.cpp[]`)
    lines.push('----')
    lines.push('')
    lines.push('[.adoc-preview]')
    lines.push('========')
    lines.push(`include::example$snippets/commands/${slug}.adoc[tags=!footer]`)
    lines.push('========')
    lines.push('')
  }
  if (entry.see_also && entry.see_also.length) {
    const refs = entry.see_also.map((name) => `<<${cmdAnchor(name)},${name}>>`).join(', ')
    lines.push(`*See also:* ${refs}`)
    lines.push('')
  }
  return lines.join('\n')
}

// Render one reference section: a `== title` heading followed by every
// command in that section, in registry order.
function renderGroup (data, title, top, subs) {
  const section = data[top] || {}
  const keys = subs || Object.keys(section)
  const parts = [`== ${title}\n`]
  for (const subKey of keys) {
    for (const entry of section[subKey] || []) {
      parts.push(renderPartial(entry))
    }
  }
  return parts.join('\n')
}

module.exports.register = function () {
  this.on('contentClassified', ({ contentCatalog }) => {
    const registryFiles = contentCatalog.findBy({
      family: 'partial',
      relative: 'commands-registry.json',
    })
    for (const registryFile of registryFiles) {
      const { component, version, module: mod, origin } = registryFile.src
      let data
      try {
        data = JSON.parse(registryFile.contents.toString())
      } catch (err) {
        this.getLogger('commands-registry-extension').warn(
          `failed to parse ${component}@${version} `
          + `partials/commands-registry.json: ${err.message}`)
        continue
      }

      const addPartial = (relative, slug, contents) => {
        const depth = relative.split('/').length
        contentCatalog.addFile({
          contents: Buffer.from(contents),
          src: {
            component,
            version,
            module: mod,
            family: 'partial',
            relative,
            basename: `${slug}.adoc`,
            extname: '.adoc',
            stem: slug,
            mediaType: 'text/asciidoc',
            path: `modules/${mod}/partials/${relative}`,
            moduleRootPath: '../..' + '/..'.repeat(depth - 1),
            origin,
          },
        })
      }

      // One partial per command, for granular includes.
      for (const [slug, entry] of Object.entries(indexRegistry(data))) {
        addPartial(`commands/${slug}.adoc`, slug, renderPartial(entry))
      }

      // One combined partial for the whole reference: every section,
      // heading and all, in order. The reference page is then a single
      // include that stays in sync with the registry automatically.
      const reference = GROUPS
        .map((g) => renderGroup(data, g.title, g.top, g.subs))
        .join('\n')
      addPartial('commands/groups/reference.adoc', 'reference', reference)
    }
  })
}
