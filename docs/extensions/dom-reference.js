/*
    Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/cppalliance/mrdocs

    Antora extension that renders the DOM Reference section of the
    docs site from mrdocs-dom-schema.json. Drop-in counterpart to
    config-options-reference.js for the JSON-Schema describing the
    Handlebars DOM.
*/

function escapeHtml(str)
{
    return str
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
}

// Convert a CamelCase / PascalCase type name into a kebab-case
// anchor id. Adjacent uppercase letters that look like an acronym
// (e.g. "TParam", "TArg") stay glued together - the existing
// manually-maintained docs use `tparam-fields` and `targ-fields`,
// not `t-param-fields` / `t-arg-fields`, and we preserve that.
function kebabAnchor(name)
{
    return name
        .replace(/([a-z0-9])([A-Z])/g, '$1-$2')
        .toLowerCase();
}

function anchorFor(typeName)
{
    return kebabAnchor(typeName) + '-fields';
}

// Strip "#/$defs/X" -> "X".
function refTypeName(ref)
{
    const prefix = '#/$defs/';
    return ref.startsWith(prefix) ? ref.substring(prefix.length) : ref;
}

// Render the type cell of a property. Returns HTML.
function describeType(prop)
{
    if (prop.$ref) {
        const t = refTypeName(prop.$ref);
        return `<a href="#${anchorFor(t)}"><code>${t}</code></a>`;
    }
    if (prop.const !== undefined) {
        return `<code>"${escapeHtml(String(prop.const))}"</code>`;
    }
    if (prop.type === 'array') {
        return `array of ${describeType(prop.items)}`;
    }
    if (prop.type === 'string' && Array.isArray(prop.enum)) {
        const values = prop.enum.map(v =>
            `<code>"${escapeHtml(v)}"</code>`).join(' &#124; ');
        return `string (${values})`;
    }
    if (prop.type === 'object') {
        return 'object';
    }
    return prop.type || 'any';
}

// Render one `$defs` entry: heading + description + members table
// (or a `oneOf` list, for polymorphic unions).
function renderTypeSection(typeName, schema, level, block)
{
    block.lines.push(
        `<div class="sect${level - 1}">`);
    block.lines.push(
        `<h${level} id="${anchorFor(typeName)}">`);
    block.lines.push(
        `<a class="anchor" href="#${anchorFor(typeName)}"></a>`);
    block.lines.push(escapeHtml(typeName));
    block.lines.push(`</h${level}>`);
    block.lines.push(`<div class="sectionbody">`);

    if (schema.description) {
        block.lines.push(
            `<div class="paragraph"><p>${escapeHtml(schema.description)}</p></div>`);
    }

    if (Array.isArray(schema.oneOf)) {
        // Polymorphic union: list each variant as a link.
        block.lines.push(`<div class="paragraph"><p>One of:</p></div>`);
        block.lines.push(`<div class="ulist"><ul>`);
        for (const variant of schema.oneOf) {
            const name = refTypeName(variant.$ref);
            block.lines.push(
                `<li><a href="#${anchorFor(name)}"><code>${escapeHtml(name)}</code></a></li>`);
        }
        block.lines.push(`</ul></div>`);
    } else if (schema.properties) {
        // Object type: render the property table.
        const required = new Set(schema.required || []);
        block.lines.push(
            `<table class="tableblock frame-all grid-all stretch">`);
        block.lines.push(`<colgroup>`);
        block.lines.push(`<col style="width: 25%;">`);
        block.lines.push(`<col style="width: 25%;">`);
        block.lines.push(`<col style="width: 50%;">`);
        block.lines.push(`</colgroup>`);
        block.lines.push(`<thead><tr>`);
        block.lines.push(
            `<th class="tableblock halign-left valign-top">Property</th>`);
        block.lines.push(
            `<th class="tableblock halign-left valign-top">Type</th>`);
        block.lines.push(
            `<th class="tableblock halign-left valign-top">Description</th>`);
        block.lines.push(`</tr></thead>`);
        block.lines.push(`<tbody>`);
        for (const [name, prop] of Object.entries(schema.properties)) {
            block.lines.push(`<tr>`);
            block.lines.push(
                `<td class="tableblock halign-left valign-top">`
                + `<code>${escapeHtml(name)}</code>`
                + (required.has(name)
                    ? ` <span style="color: orangered;">(required)</span>`
                    : '')
                + `</td>`);
            block.lines.push(
                `<td class="tableblock halign-left valign-top">${describeType(prop)}</td>`);
            block.lines.push(
                `<td class="tableblock halign-left valign-top">`
                + (prop.description ? escapeHtml(prop.description) : '')
                + `</td>`);
            block.lines.push(`</tr>`);
        }
        block.lines.push(`</tbody>`);
        block.lines.push(`</table>`);
    }

    block.lines.push(`</div>`); // sectionbody
    block.lines.push(`</div>`); // sect
}

module.exports = function (registry) {
    if (!registry) {
        throw new Error('registry must be defined');
    }
    registry.block('dom-reference', function () {
        const self = this;
        self.onContext('example');
        self.process((parent, reader, attrs) => {
            const level = parseInt(attrs.level || 3, 10);
            const code = reader.getLines().join('\n');
            const schema = JSON.parse(code);
            const block = self.$create_pass_block(parent, '', Opal.hash(attrs));
            const defs = schema['$defs'] || {};
            for (const [typeName, def] of Object.entries(defs)) {
                renderTypeSection(typeName, def, level, block);
            }
            return block;
        });
    });
};
