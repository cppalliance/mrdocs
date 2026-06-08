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
const hljs = require('highlight.js/lib/core');
hljs.registerLanguage('cpp', require('highlight.js/lib/languages/cpp'));
hljs.registerLanguage('xml', require('highlight.js/lib/languages/xml'));
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
for (let panel of data.panels) {
    console.log(`Generating documentation for panel ${panel.source}`)

    // Find source file
    const sourcePath = path.join(absSnippetsDir, panel.source)
    assert(sourcePath.endsWith('.cpp'))
    assert(fs.existsSync(sourcePath))
    const sourceBasename = path.basename(sourcePath, path.extname(sourcePath))

    // Run mrdocs in header-scan mode: no compilation database, the
    // config points `input` at the snippets directory, `file-patterns`
    // narrows the scan to this panel's source file, and `recursive`
    // is off so we never pick up identically named files in nested
    // golden-test subdirectories.
    const mrdocsConfig = path.join(absSnippetsDir, 'mrdocs.yml')
    const mrdocsOutput = path.join(os.tmpdir(), `mrdocs-website-${sourceBasename}.html`)
    const args = [
        mrdocsExecutable,
        `--config=${mrdocsConfig}`,
        `--file-patterns=${panel.source}`,
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
    panel.documentation = fs.readFileSync(mrdocsOutput, 'utf8');

    // Also inject the contents of the source file as highlighted C++
    const snippetContents = fs.readFileSync(sourcePath, 'utf8');
    panel.snippet = hljs.highlight(snippetContents, {language: 'cpp'}).value;

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
