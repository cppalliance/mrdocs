/*
    Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/cppalliance/mrdocs
*/

const fs = require('fs')
const path = require('path')

// Optional per-option fixture under
// `test-files/golden-tests/snippets/options/<option>/`. The extension
// uses this to emit an `<em>Example</em>` block under each option's
// detail, with the .cpp source and the .yml configuration as code
// blocks. Rendering the .adoc preview inline would require a second
// asciidoctor instance and clashes with Antora's, so the .adoc
// preview is handled by the per-category page using `include::`
// directives instead.
const OPTIONS_FIXTURES_DIR = path.resolve(
    __dirname, '..', '..', 'test-files', 'golden-tests', 'snippets', 'options')

const ADOC_HEADER_RE = /^= Reference\n:mrdocs:\n\n/
const ADOC_FOOTER_RE = /\n*\[\.small\]#Created with https:\/\/www\.mrdocs\.com\[MrDocs\]#\s*\n*$/

// Remove every AsciiDoc-include tag region from `text`. A tag region
// is the block between `<prefix> tag::name[]` and `<prefix> end::name[]`
// (markers and their enclosed content). The markers exist so that the
// intermediary pages can include only the relevant fragments via
// `[tags=!test-only]` and similar; they are noise when the whole file
// is dumped into the auto-generated reference preview.
function stripTaggedRegions(text) {
    return text
        .replace(/^[ \t]*(?:\/\/|#)[ \t]*tag::([A-Za-z0-9_\-]+)\[\][ \t]*\r?\n[\s\S]*?^[ \t]*(?:\/\/|#)[ \t]*end::\1\[\][ \t]*\r?\n?/gm, '')
        // Tidy: collapse 3+ consecutive blank lines that a removal can leave.
        .replace(/\n{3,}/g, '\n\n')
        .replace(/^\n+/, '')
}

// Return the lines between `<prefix> tag::body` and `<prefix> end::body`,
// without the marker lines themselves. Falls back to the whole file
// when no body tag is present.
function extractBody(text) {
    const m = text.match(/(?:^|\n)[ \t]*(?:\/\/|#)[ \t]*tag::body\[\][ \t]*\r?\n([\s\S]*?)[ \t]*(?:\/\/|#)[ \t]*end::body\[\]/m)
    return m ? m[1].replace(/\n+$/, '') : stripTaggedRegions(text).replace(/\n+$/, '')
}

// Collect every `*.hpp`/`*.h` under the fixture's `include/` tree, if
// any. The fixture's `.cpp` is often just a couple of `#include`s, so
// the meaningful declarations live in these headers; we display them
// alongside the .cpp so the example actually makes sense.
function loadIncludeHeaders(dir) {
    const includeRoot = path.join(dir, 'include')
    if (!fs.existsSync(includeRoot)) return []
    const out = []
    const walk = (subdir) => {
        for (const entry of fs.readdirSync(subdir, { withFileTypes: true })) {
            const full = path.join(subdir, entry.name)
            if (entry.isDirectory()) {
                walk(full)
            } else if (/\.(hpp|h|hxx|ipp)$/i.test(entry.name)) {
                const rel = path.relative(dir, full).split(path.sep).join('/')
                out.push({ path: rel, body: extractBody(fs.readFileSync(full, 'utf-8')) })
            }
        }
    }
    walk(includeRoot)
    out.sort((a, b) => a.path.localeCompare(b.path))
    return out
}

function loadOptionFixture(optionName) {
    const dir = path.join(OPTIONS_FIXTURES_DIR, optionName)
    if (!fs.existsSync(dir)) return null
    const read = (ext) => {
        const p = path.join(dir, `${optionName}.${ext}`)
        return fs.existsSync(p) ? fs.readFileSync(p, 'utf-8') : null
    }
    const adocRaw = read('adoc')
    const cppRaw = read('cpp')
    const ymlRaw = read('yml')
    return {
        cpp: cppRaw ? stripTaggedRegions(cppRaw) : null,
        yml: ymlRaw ? stripTaggedRegions(ymlRaw) : null,
        adoc: adocRaw
            ? adocRaw.replace(ADOC_HEADER_RE, '').replace(ADOC_FOOTER_RE, '')
            : null,
        headers: loadIncludeHeaders(dir),
    }
}

// Asciidoctor instance reused across calls. `@asciidoctor/core` is
// the package Antora itself loads, so requiring it doesn't create a
// second Opal runtime the way the top-level `asciidoctor` package
// would.
let _adoc = null
function asciidoctorCore() {
    if (!_adoc) _adoc = require('@asciidoctor/core')()
    return _adoc
}

function renderFixtureHtml(optionName) {
    const f = loadOptionFixture(optionName)
    if (!f) return null
    const parts = []
    parts.push('<div class="paragraph"><p><em>Example</em></p></div>')
    // Headers under `include/` come first: that is where the actual
    // declarations live, while the `.cpp` is usually just a couple of
    // `#include`s to wire them into a translation unit.
    for (const h of f.headers) {
        parts.push(`<div class="listingblock"><div class="title">${escapeHtml(h.path)}</div><div class="content">`)
        parts.push(`<pre class="highlightjs highlight"><code class="language-cpp hljs" data-lang="cpp">${escapeHtml(h.body)}</code></pre>`)
        parts.push('</div></div>')
    }
    if (f.cpp && !/^\s*$/.test(f.cpp)) {
        const cppTitle = f.headers.length ? `${optionName}.cpp` : 'Input'
        parts.push(`<div class="listingblock"><div class="title">${escapeHtml(cppTitle)}</div><div class="content">`)
        parts.push(`<pre class="highlightjs highlight"><code class="language-cpp hljs" data-lang="cpp">${escapeHtml(f.cpp.replace(/\n+$/, ''))}</code></pre>`)
        parts.push('</div></div>')
    }
    if (f.yml) {
        parts.push('<div class="listingblock"><div class="title">mrdocs.yml</div><div class="content">')
        parts.push(`<pre class="highlightjs highlight"><code class="language-yaml hljs" data-lang="yaml">${escapeHtml(f.yml.replace(/\n+$/, ''))}</code></pre>`)
        parts.push('</div></div>')
    }
    if (f.adoc) {
        try {
            // `leveloffset=2` so the embedded symbol page's `==` headings
            // render as `<h4>` instead of `<h2>`. That stops the embedded
            // title from colliding with the absolute-positioned preview
            // label and matches the `.adoc-preview h4/h5/h6` rules in
            // `adoc-preview.css`.
            let html = asciidoctorCore().convert(f.adoc, {
                standalone: false,
                attributes: { leveloffset: '+2' },
            })
            // Tag every section heading inside the preview with
            // `class="discrete"`. The convert() output emits plain
            // `<h4 id="...">name</h4>`, which the host page's
            // `.doc h5:not(.discrete) { text-transform: uppercase }`
            // rule (and its h6 sibling) would render in all caps. The
            // sibling preview path through `[.adoc-preview]` blocks
            // already lands at `class="discrete"` via
            // `adoc-preview-extension.js`, so adding it here makes the
            // two paths render identically.
            html = html.replace(
                /<h([4-6])(\s+[^>]*)?>/g,
                (_match, level, attrs = '') =>
                    /\bclass\s*=/.test(attrs)
                        ? `<h${level}${attrs.replace(/class="([^"]*)"/, (_m, c) => `class="${c} discrete"`)}>`
                        : `<h${level}${attrs} class="discrete">`
            )
            // The inline title becomes the subtitle next to the
            // canonical "Preview" label that `adoc-preview-extension.js`
            // injects post-conversion: the strip reads "PREVIEW ·
            // <option>" so the reader sees which option this preview is
            // demonstrating. Emitting a label here too would stack two
            // labels at top:0 (the original overlap bug).
            const title = `<div class="title">${escapeHtml(optionName)}</div>`
            parts.push(`<div class="exampleblock adoc-preview">${title}<div class="content">`)
            parts.push(html)
            parts.push('</div></div>')
        } catch (err) {
            console.error('[config-options-reference] failed to render preview for', optionName, ':', err.message)
        }
    }
    return parts.join('\n')
}

function toSnakeCase(str) {
    return str.toLowerCase().replace(/ /g, '_').replace(/[^a-z0-9_]/g, '');
}

function escapeHtml(str)
{
    return str
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
}

function replaceCodeTags(str) {
    return str.replace(/`([^`\n]+)`/g, '<code>$1</code>')
}

function toTypeStr(type) {
    if (type === undefined) {
        return 'object'
    }
    if (type === 'string') {
        return 'string'
    }
    if (type === 'string-list') {
        return 'string or list of strings'
    }
    if (type === 'int') {
        return 'integer'
    }
    if (type === 'unsigned') {
        return 'unsigned integer'
    }
    if (type === 'bool') {
        return 'boolean'
    }
    if (type === 'path') {
        return 'path'
    }
    if (type === 'dir-path') {
        return 'directory path'
    }
    if (type === 'file-path') {
        return 'file path'
    }
    if (type.startsWith('list<')) {
        return 'list of ' + toTypeStr(type.substring(5, type.length - 1)) + 's'
    }
    if (type === 'map<string,string>') {
        return 'object of strings'
    }
    if (type === 'map<string,object>') {
        return 'object of objects'
    }
    return type
}

function toDefaultValueStr(value) {
    if (value === undefined) {
        return ''
    }
    let valueIsArray = Array.isArray(value)
    if (valueIsArray) {
        let valueStrs = value.map(v => toDefaultValueStr(v))
        return `[${valueStrs.join(', ')}]`
    }
    let valueIsObject = typeof value === 'object'
    if (valueIsObject) {
        let valueStrs = Object.keys(value).map(k => `${k}: ${toDefaultValueStr(value[k])}`)
        return `{${valueStrs.join(', ')}}`
    }
    let valueIsBool = typeof value === 'boolean'
    if (valueIsBool) {
        return `<span style="color: darkblue;">${value}</span>`
    }
    let valueIsString = typeof value === 'string'
    if (valueIsString) {
        if (value === '') {
            return ''
        }
        return `<span style="color: darkgreen;">"${escapeHtml(value)}"</span>`
    }
    let valueIsNumber = typeof value === 'number'
    if (valueIsNumber) {
        return `<span style="color: darkblue;">${value}</span>`
    }
    return `${value}`

}

function pushOptionBlocks(options, block, parents = []) {
    function makeOptionID(option) {
        return [...parents, option.name].join('_') + "_option"
    }

    block.lines.push('<table class="tableblock frame-all grid-all stretch">')
    block.lines.push('<colgroup>')
    block.lines.push('<col style="width: 23.3333%;">')
    block.lines.push('<col style="width: 46.6667%;">')
    block.lines.push('<col style="width: 30%;">')
    block.lines.push('</colgroup>')
    block.lines.push('<thead>')
    block.lines.push('<tr>')
    block.lines.push('<th class="tableblock halign-left valign-top">Name</th>')
    block.lines.push('<th class="tableblock halign-left valign-top">Description</th>')
    block.lines.push('<th class="tableblock halign-left valign-top">Default</th>')
    block.lines.push('</tr>')
    block.lines.push('</thead>')
    block.lines.push('<tbody>')
    for (let option of options) {
        let optionName = [...parents, option.name].join('.')
        block.lines.push('<tr>')
        block.lines.push(`<td class="tableblock halign-left valign-top">`)
        const colorStr = option['deprecated'] ? 'red' : 'darkblue'
        block.lines.push(`<a href="#${makeOptionID(option)}"><code style="color: ${colorStr}">${optionName}</code></a>`)
        block.lines.push(`<br/>`)
        block.lines.push(`<span style="color: darkgreen;">(${toTypeStr(option.type)})</span>`)
        let observations = []
        if (option.required) {
            observations.push('Required')
        }
        if (option['command-line-only']) {
            observations.push('Command line only')
        }
        if (option['deprecated']) {
            observations.push(`Deprecated`)
        }
        if (observations.length !== 0) {
            block.lines.push(`<br/>`)
            let observationsStr = observations.join(', ')
            block.lines.push(`<span style="color: orangered;">(${observationsStr})</span>`)
        }
        block.lines.push(`</td>`)
        block.lines.push(`<td class="tableblock halign-left valign-top">${option.brief}</td>`)
        block.lines.push(`<td class="tableblock halign-left valign-top">${toDefaultValueStr(option.default)}</td>`)
        block.lines.push('</tr>')
    }
    block.lines.push('</tbody>')
    block.lines.push('</table>')

    // Option details
    for (let option of options) {
        let optionName = [...parents, option.name].join('.')
        const optionID = optionName.replace(/\./g, '_')
        const colorStr = option['deprecated'] ? 'red' : 'darkblue'
        block.lines.push(`<div class="paragraph" id="${makeOptionID(option)}"><p><b><code style="color: ${colorStr}">${optionName}</code></b></p></div>`)
        block.lines.push(`<div class="paragraph"><p><i>${option.brief}</i></p></div>`)
        if (option.details) {
            block.lines.push(`<div class="paragraph"><p>${replaceCodeTags(escapeHtml(option.details))}</p></div>`)
        }
        block.lines.push(`<div class="paragraph"><p>`)
        block.lines.push(`<div class="ulist">`)
        block.lines.push(`<ul>`)
        if (option['deprecated']) {
            block.lines.push(`<li><span style="color: red;">Deprecated</span>: ${replaceCodeTags(escapeHtml(option['deprecated']))}</li>`)
        }
        if (option.type) {
            block.lines.push(`<li>Type: ${toTypeStr(option.type)}</li>`)
        } else {
            block.lines.push(`<li>Type: object (See below)</li>`)
        }
        if (option.required) {
            block.lines.push(`<li><span style="color: orangered;">Required</span></li>`)
        }
        if (option['command-line-only']) {
            block.lines.push(`<li>Command line only</li>`)
        }
        if (option['must-exits']) {
            block.lines.push(`<li>The path must exist</li>`)
        }
        if (option.default !== undefined) {
            block.lines.push(`<li>Default value: ${toDefaultValueStr(option.default)}</li>`)
        }
        if (option.type === 'enum') {
            block.lines.push(`<li>Allowed values: ${toDefaultValueStr(option.values)}</li>`)
        }
        if (option['command-line-sink']) {
            block.lines.push(`<li>This command is a command line sink. Any command line argument that is not recognized by the parser will be passed to this command.</li>`)
        }
        if (option['min-value'] || option.type === 'unsigned') {
            block.lines.push(`<li>Minimum value: ${toDefaultValueStr(option['min-value'] || 0)}</li>`)
        }
        if (option['max-value']) {
            block.lines.push(`<li>Maximum value: ${toDefaultValueStr(option['max-value'])}</li>`)
        }
        if (option.relativeto) {
            block.lines.push(`<li>Relative paths are relative to: ${toDefaultValueStr(escapeHtml(option.relativeto))}</li>`)
        }
        block.lines.push(`</ul>`)
        block.lines.push(`</div>`)
        block.lines.push(`</p></div>`)

        // Embed the per-option fixture (if any) right under the
        // option's detail block. This keeps the single source of
        // truth: the option is described once in the auto-generated
        // detail above, and the example below shows it in action.
        const fixtureHtml = renderFixtureHtml(option.name)
        if (fixtureHtml) {
            block.lines.push(fixtureHtml)
        }
    }

    // Iterate the options that have suboptions
    for (let option of options) {
        if (!option.options) {
            continue
        }
        block.lines.push(`<div class="paragraph"><p><b><code>${option.name}</code> suboptions</b></p></div>`)
        pushOptionBlocks(option.options, block, [...parents, option.name]);
    }
}

module.exports = function (registry) {
    // Make sure registry is defined
    if (!registry) {
        throw new Error('registry must be defined');
    }

    registry.block('config-options-reference', function () {
        const self = this
        self.onContext('example')
        let blocks = []
        self.process((parent, reader, attrs) => {
            let level = attrs.level || 3
            let code = reader.getLines().join('\n')
            let categories = JSON.parse(code)
            // Optional filter: render only the named category. Used by
            // the per-category reference pages so each one only emits
            // the options for its own section, instead of the full
            // reference.
            let categoryFilter = attrs.category
            if (categoryFilter) {
                categories = categories.filter(c => c.category === categoryFilter)
                if (!categories.length) {
                    throw new Error(`config-options-reference: no category named "${categoryFilter}" (available: ${JSON.parse(code).map(c => c.category).join(', ')})`)
                }
            }
            // When emitting a single category page, the category name
            // is already the page title; suppress the inner heading
            // to avoid the duplicated title.
            let omitHeading = !!attrs['omit-heading']
            let block = self.$create_pass_block(parent, '', Opal.hash(attrs))
            // Emit each category as a top-level `.sect1` so it matches
            // Antora's TOC selector (`article.doc > .sect1 > h2[id]`)
            // and shows up in the right-hand sidebar.
            for (let category of categories) {
                block.lines.push('<div class="sect1">')
                if (!omitHeading) {
                    let snake_case = toSnakeCase(category.category)
                    block.lines.push(`<h${level} id="_${snake_case}_options_reference">`)
                    block.lines.push(`<a class="anchor" href="#_${snake_case}_options_reference"></a>`)
                    block.lines.push(category.category)
                    block.lines.push(`</h${level}>`)
                }
                if (category.brief) {
                    block.lines.push(`<div class="paragraph"><p><i>${category.brief}</i></p></div>`)
                }
                if (category.details) {
                    block.lines.push(`<div class="paragraph"><p>${replaceCodeTags(escapeHtml(category.details))}</p></div>`)
                }
                pushOptionBlocks(category.options, block);
                block.lines.push(`</div>`)
            }
            return block
        })
    })
}
