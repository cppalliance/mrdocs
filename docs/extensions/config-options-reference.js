/*
    Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/cppalliance/mrdocs
*/

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
        block.lines.push(`<code style="color: darkblue">${optionName}</code>`)
        block.lines.push(`<br/>`)
        block.lines.push(`<span style="color: darkgreen;">(${toTypeStr(option.type)})</span>`)
        let observations = []
        if (option.required) {
            observations.push('Required')
        }
        if (option['command-line-only']) {
            observations.push('Command line only')
        }
        if (observations.length !== 0) {
            block.lines.push(`<br/><br/>`)
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
        block.lines.push(`<div class="paragraph"><p><b><code style="color: darkblue">${optionName}</code></b></p></div>`)
        block.lines.push(`<div class="paragraph"><p><i>${option.brief}</i></p></div>`)
        if (option.details) {
            block.lines.push(`<div class="paragraph"><p>${replaceCodeTags(escapeHtml(option.details))}</p></div>`)
        }
        block.lines.push(`<div class="paragraph"><p>`)
        block.lines.push(`<div class="ulist">`)
        block.lines.push(`<ul>`)
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
            let block = self.$create_pass_block(parent, '', Opal.hash(attrs))
            for (let category of categories) {
                block.lines.push('<div class="sect2">')
                let snake_case = toSnakeCase(category.category)
                block.lines.push(`<h${level} id="_${snake_case}_options_reference">`)
                block.lines.push(`<a class="anchor" href="#_${snake_case}_options_reference"></a>`)
                block.lines.push(category.category)
                block.lines.push(`</h${level}>`)
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
