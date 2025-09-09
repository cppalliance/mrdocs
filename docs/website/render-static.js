const Handlebars = require('handlebars');
const hljs = require('highlight.js/lib/core');
hljs.registerLanguage('cpp', require('highlight.js/lib/languages/cpp'));
hljs.registerLanguage('xml', require('highlight.js/lib/languages/xml'));
const fs = require('fs');
const path = require('path');

// Read the template file
const templateFile = 'index.html.hbs';
let templateSource = fs.readFileSync(templateFile, 'utf8');

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

// Generate static documentation for each panel
const absSnippetsDir = path.join(__dirname, 'snippets')
for (let panel of data.panels) {
    console.log(`Processing panel ${panel.source}`)

    // Find source file
    const sourcePath = path.join(absSnippetsDir, panel.source)
    if (!fs.existsSync(sourcePath)) {
        console.log(`Source file not found: ${sourcePath}`)
        continue;
    }

    // Create simple static documentation
    const snippetContents = fs.readFileSync(sourcePath, 'utf8');
    const highlightedSnippet = hljs.highlight(snippetContents, {language: 'cpp'}).value;
    
    // Create a simple HTML documentation
    panel.documentation = `
        <div class="documentation">
            <h3>Code Documentation</h3>
            <div class="code-block">
                <pre><code class="language-cpp">${highlightedSnippet}</code></pre>
            </div>
            <p>This code demonstrates ${panel.description.toLowerCase()}.</p>
        </div>
    `;
    
    // Also inject the contents of the source file as highlighted C++
    panel.snippet = highlightedSnippet;

    console.log(`Documentation processed successfully for panel ${panel.source}`)
}

// Replace the logo partial with actual content
let logoPath = path.join(__dirname, '..', 'ui', 'src', 'partials', 'logo.hbs');
let logoContent = fs.readFileSync(logoPath, 'utf8');
templateSource = templateSource.replace(/\s*\{\{>\s*logo\s*\}\}\s*/g, logoContent);

// Compile the template AFTER replacement
let template = Handlebars.compile(templateSource);

// Render the template with the data containing theb snippet data
const result = template(data);

// Write the rendered website template to an HTML file
const outputFile = templateFile.slice(0, -4);
fs.writeFileSync(outputFile, result);

console.log(`Template rendered successfully and saved to ${outputFile}`);
