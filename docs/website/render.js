const Handlebars = require('handlebars');
const hljs = require('highlight.js/lib/core');
hljs.registerLanguage('cpp', require('highlight.js/lib/languages/cpp'));
hljs.registerLanguage('xml', require('highlight.js/lib/languages/xml'));
const fs = require('fs');
const assert = require('assert');
const path = require('path');
const {execSync} = require('child_process');


// Read the template file
const templateFile = 'index.html.hbs';
const templateSource = fs.readFileSync(templateFile, 'utf8');

// Compile the template
const template = Handlebars.compile(templateSource);

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
    process.exit(1);
}

// Read panel snippet files and create documentation
const absSnippetsDir = path.join(__dirname, 'snippets')
for (let panel of data.panels) {
    // Find source file
    const sourcePath = path.join(absSnippetsDir, panel.source)
    assert(sourcePath.endsWith('.cpp'))
    assert(fs.existsSync(sourcePath))
    const sourceBasename = path.basename(sourcePath, path.extname(sourcePath))

    // Create a CMakeLists.txt file for the snippet
    const cmakeListsPath = path.join(absSnippetsDir, 'CMakeLists.txt')
    const cmakeListsContent = `
cmake_minimum_required(VERSION 3.13)
project(${sourceBasename})
add_executable(${sourceBasename} ${panel.source})
target_compile_features(${sourceBasename} PRIVATE cxx_std_23)
`;
    fs.writeFileSync(cmakeListsPath, cmakeListsContent)

    // Run mrdocs to generate documentation
    const mrdocsConfig = path.join(absSnippetsDir, 'mrdocs.yml')
    const mrdocsInput = cmakeListsPath
    const mrdocsOutput = path.join(absSnippetsDir, 'output')
    const args = [
        mrdocsExecutable,
        `--config=${mrdocsConfig}`,
        mrdocsInput,
        `--output=${mrdocsOutput}`,
        '--multipage=true',
        '--generate=html',
        '--embedded=true',
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
    const documentationFilename = `${sourceBasename}.html`
    const documentationPath = path.join(mrdocsOutput, documentationFilename)
    if (!fs.existsSync(documentationPath)) {
        console.log(`Documentation file ${documentationFilename} not found in ${mrdocsOutput}`)
        console.log('Failed to generate website panel documentation')
        process.exit(1)
    }
    panel.documentation = fs.readFileSync(documentationPath, 'utf8');

    // Also inject the contents of the source file as highlighted C++
    const snippetContents = fs.readFileSync(sourcePath, 'utf8');
    const highlightedSnippet = hljs.highlight(snippetContents, {language: 'cpp'}).value;
    panel.snippet = highlightedSnippet;

    // Delete these temporary files
    fs.rmSync(mrdocsOutput, {recursive: true});
    fs.unlinkSync(cmakeListsPath);
}

// Render the template with the data containing the snippet data
const result = template(data);

// Write the rendered website template to an HTML file
assert(templateFile.endsWith('.hbs'));
const outputFile = templateFile.slice(0, -4);
fs.writeFileSync(outputFile, result);

console.log(`Template rendered successfully and saved to ${outputFile}`);
