//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

const Handlebars = require('handlebars');
// Reuse the doc site's rich C++ language definition (the doc-comment
// colouring, etc.) instead of the stock cpp language, so the landing page
// snippets look identical to the documentation. highlight.js and its cpp
// language come from this package's own dependency (pinned to 9.x to match
// the UI bundle); we only borrow the doc-comment logic from docs/ui, passing
// our base cpp definition into it, so nothing here has to resolve
// highlight.js from docs/ui at run time.
const hljs = require('highlight.js/lib/highlight');
const { makeRichCpp } = require('../ui/src/js/vendor/mrdocs-highlight-languages.js');
hljs.registerLanguage('cpp', makeRichCpp(require('highlight.js/lib/languages/cpp')));
const fs = require('fs');
const os = require('os');
const assert = require('assert');
const path = require('path');
const {execSync} = require('child_process');

// Read the template file
const templateFile = 'index.html.hbs';
let templateSource = fs.readFileSync(templateFile, 'utf8');

// Compile the template

// Register helpers
const helpersDir = path.join(__dirname, 'helpers');
const helpersDirExists = fs.existsSync(helpersDir) && fs.lstatSync(helpersDir).isDirectory();
if (helpersDirExists) {
    fs.readdirSync(helpersDir).forEach(file => {
        const helperName = path.basename(file, path.extname(file));
        const helperFunction = require(path.join(helpersDir, file));
        Handlebars.registerHelper(helperName, helperFunction);
    });
}

// Read the JSON data file
const dataFile = 'data.json';
const dataContents = fs.readFileSync(dataFile, 'utf8')
const data = JSON.parse(dataContents);

// Generate documentation for each panel
const mrdocsRoot = process.env.MRDOCS_ROOT;
if (!mrdocsRoot) {
    console.log('MRDOCS_ROOT environment variable is not set');
    process.exit(1);
}
const mrdocsExecutable = path.join(mrdocsRoot, 'bin', 'mrdocs') + (process.platform === 'win32' ? '.exe' : '');
if (!fs.existsSync(mrdocsExecutable)) {
    console.log(`mrdocs executable not found at ${mrdocsExecutable}`);
    // Walk up the path to find the first directory that exists
    let dir = path.dirname(mrdocsExecutable);
    while (dir && dir !== path.dirname(dir)) {
        if (fs.existsSync(dir)) {
            console.log(`Nearest existing directory: ${dir}`);
            console.log(`Contents: ${fs.readdirSync(dir).join(', ')}`);
            break;
        }
        dir = path.dirname(dir);
    }
    process.exit(1);
}

// Read panel snippet files and create documentation. The default
// location is the golden-tests snippets directory, so the landing
// page reuses the same sources the test suite already covers.
// Override with SNIPPETS_PATH to point at a different snippet root.
const absSnippetsDir = process.env.SNIPPETS_PATH
    ? path.resolve(process.env.SNIPPETS_PATH)
    : path.resolve(__dirname, '..', '..', 'test-files', 'golden-tests', 'snippets')

// The mrdocs HTML generator emits synopsis code blocks as
// <pre><code class="source-code cpp">...</code></pre> and leaves them
// unhighlighted; the doc site colours code through Antora's pipeline, not
// this generator, so nothing highlights them on the landing page. Run the
// same highlighter used for the raw snippet over those blocks (their
// contents are HTML-escaped C++, so unescape before highlighting and let
// highlight.js re-escape).
function highlightSynopsisBlocks (html) {
    return html.replace(
        /<pre><code class="source-code cpp">([\s\S]*?)<\/code><\/pre>/g,
        (_match, escaped) => {
            const code = escaped
                .replace(/&lt;/g, '<')
                .replace(/&gt;/g, '>')
                .replace(/&quot;/g, '"')
                .replace(/&#39;/g, "'")
                .replace(/&amp;/g, '&')
            // Keep the original class (no `hljs`): the CDN github theme
            // paints `.hljs` with a white background, which on this dark page
            // turns the block into a white box. The inner hljs-* spans are
            // styled by standalone class rules, so they colour without it,
            // exactly like the raw snippet above.
            return '<pre><code class="source-code cpp">' +
                hljs.highlight('cpp', code).value +
                '</code></pre>'
        }
    )
}

for (let panel of data.panels) {
    console.log(`Generating documentation for panel ${panel.source}`)

    // Find source file. The .cpp is a thin translation unit that only
    // includes the same-named .hpp; the documented declarations live in
    // that header, which is also what we show as the example (we document
    // headers, not .cpp files).
    const sourcePath = path.join(absSnippetsDir, panel.source)
    assert(sourcePath.endsWith('.cpp'))
    assert(fs.existsSync(sourcePath))
    const sourceBasename = path.basename(sourcePath, path.extname(sourcePath))
    const headerSource = panel.source.replace(/\.cpp$/, '.hpp')
    const headerPath = path.join(absSnippetsDir, headerSource)
    assert(fs.existsSync(headerPath))

    // Run mrdocs in header-scan mode: no compilation database, the
    // config points `input` at the snippets directory, `file-patterns`
    // matches this panel's .cpp and its .hpp (the .cpp is the translation
    // unit; the declarations live in the header), and `recursive` is off
    // so we never pick up identically named files in nested golden-test
    // subdirectories.
    const mrdocsConfig = path.join(absSnippetsDir, 'mrdocs.yml')
    const mrdocsOutput = path.join(os.tmpdir(), `mrdocs-website-${sourceBasename}.html`)
    const args = [
        mrdocsExecutable,
        `--config=${mrdocsConfig}`,
        `--file-patterns=${sourceBasename}.*`,
        '--recursive=false',
        `--output=${mrdocsOutput}`,
        '--multipage=false',
        '--generator=html',
        '--embedded=true',
        '--show-namespaces=false',
        '--tagfile=',
    ];
    const command = args.join(' ');
    console.log(`Running command: ${command}`)
    try {
        execSync(command, {stdio: 'inherit'});
    } catch (error) {
        console.error(`Command failed with exit code ${error.status}`);
        process.exit(error.status);
    }

    // Look load symbol page in the output directory
    if (!fs.existsSync(mrdocsOutput)) {
        console.log(`Documentation file not found in ${mrdocsOutput}`)
        console.log('Failed to generate website panel documentation')
        process.exit(1)
    }
    panel.documentation = highlightSynopsisBlocks(fs.readFileSync(mrdocsOutput, 'utf8'));

    // Also inject the header contents as highlighted C++ (the representative
    // example is the header, not the thin .cpp that includes it).
    const snippetContents = fs.readFileSync(headerPath, 'utf8');
    panel.snippet = hljs.highlight('cpp', snippetContents).value;

    // Delete the temporary output file
    fs.unlinkSync(mrdocsOutput);

    console.log(`Documentation generated successfully for panel ${panel.source}`)
    console.log(`====================================`)
}

// Replace the logo partial with actual content
let logoPath = path.join(__dirname, '..', 'ui', 'src', 'partials', 'logo.hbs');
let logoContent = fs.readFileSync(logoPath, 'utf8');
templateSource = templateSource.replace(/\s*\{\{>\s*logo\s*\}\}\s*/g, logoContent);

// Compile the template AFTER replacement
let template = Handlebars.compile(templateSource);

// Render the template with the data containing the snippet data
const result = template(data);

// Write the rendered website template to an HTML file
assert(templateFile.endsWith('.hbs'));
const outputFile = templateFile.slice(0, -4);
fs.writeFileSync(outputFile, result);

console.log(`Template rendered successfully and saved to ${outputFile}`);
