'use strict'

//
// Inline AsciiDoc preview.
//
// A page renders a self-contained AsciiDoc fragment as a preview by
// wrapping it in a delimited block tagged `.adoc-preview`:
//
//   .Optional custom label
//   [.adoc-preview]
//   ========
//   include::example$path/to/snippet.adoc[]
//
//   [.caption]
//   Optional caption text shown below the preview.
//   ========
//
// Use eight `=` for the wrapper, not the usual four. AsciiDoc closes a
// delimited block as soon as it sees a same-length delimiter, so a
// four-equals `[NOTE]` admonition emitted inside the rendered snippet,
// or a six-equals `[tabs]` container around the wrapper, would
// otherwise cut the preview short. Eight is the smallest even length
// that survives both cases.
//
// The block title, if present, becomes the card's header label.
// Without a title the label defaults to "Preview". Any paragraph
// inside the block with the `[.caption]` role renders as a muted
// caption below the preview content.
//
// AsciiDoc demotes section titles inside delimited blocks to
// paragraphs (e.g. `== name` lands in the HTML as
// `<div class="paragraph"><p>== name</p></div>`). This extension
// runs after the AsciiDoc -> HTML pass and rewrites those demoted
// paragraphs back to heading tags with `class="discrete"`, so the
// preview headings don't enter the page TOC.
//
// Companion CSS (`docs/ui/src/css/adoc-preview.css`) styles the
// `.adoc-preview` block as a glass card and tunes the heading sizes
// inside it. The card is intentionally a self-contained context, so
// the heading levels here are fixed rather than tracking the host
// page's section depth.
//

// `[#anchor]\n== foo` becomes `<div id="anchor" class="paragraph">
// <p>== foo</p></div>`; without the anchor the wrapping `<div>` has
// no `id`.
const DEMOTED_HEADING =
  /<div(?:\s+id="([^"]+)")?\s+class="paragraph">\s*<p>(=+)\s+(.+?)<\/p>\s*<\/div>/g

const LEVEL_TO_TAG = { 2: 'h5', 3: 'h6' }

// The opening tag of the example block plus the (optional)
// AsciiDoc-rendered title that immediately follows it. Captured as
// the first group; missing when the author did not give the block
// a title.
const PREVIEW_OPEN_WITH_TITLE =
  /<div class="exampleblock adoc-preview">\s*(?:<div class="title">(.*?)<\/div>\s*)?/g

function undemoteHeadings (html) {
  return html.replace(DEMOTED_HEADING, (_match, id, equals, text) => {
    const tag = LEVEL_TO_TAG[equals.length] || 'h6'
    const idAttr = id ? ` id="${id}"` : ''
    return `<${tag}${idAttr} class="discrete">${text}</${tag}>`
  })
}

// AsciiDoc auto-prefixes example-block titles with the
// `example-caption` attribute and a counter (e.g. "Example 1. "),
// which is noise on a preview card; strip it so the label shows
// exactly what the author wrote in the `.Title` line.
const EXAMPLE_CAPTION_PREFIX = /^Example \d+\.\s+/

// The strip always shows "Preview" as the affordance, so a reader
// can tell at a glance that the card holds rendered output. When
// the author supplies a block title, it joins the strip as a
// subtitle next to the "Preview" tag, never replacing it.
function normalizeLabels (html) {
  return html.replace(PREVIEW_OPEN_WITH_TITLE, (_match, title) => {
    const subtitle = title ? title.replace(EXAMPLE_CAPTION_PREFIX, '') : ''
    const label = subtitle
      ? `<span class="primary">Preview</span>` +
        `<span class="separator">·</span>` +
        `<span class="subtitle">${subtitle}</span>`
      : `<span class="primary">Preview</span>`
    return `<div class="exampleblock adoc-preview">` +
      `<div class="adoc-preview-label">${label}</div>`
  })
}

module.exports.register = function () {
  this.once('documentsConverted', ({ contentCatalog }) => {
    for (const page of contentCatalog.findBy({ family: 'page' })) {
      if (!page.contents) continue
      const html = page.contents.toString()
      if (!html.includes('class="exampleblock adoc-preview"')) continue
      page.contents = Buffer.from(normalizeLabels(undemoteHeadings(html)))
    }
  })
}
