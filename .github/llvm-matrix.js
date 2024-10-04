// Description: This script reads the matrix.json file and filters
// out entries that already have an existing llvm-archive-filename on the server.
// This creates a filtered matrix used to upload new archives to the server, but
// only the ones that don't already exist.

const fs = require('fs');
const core = require('@actions/core');
const { exec } = require('child_process');

// Function to check if a file exists on the server
const fileExists = (url) => {
    return new Promise((resolve) => {
        exec(`curl -s -o /dev/null -w "%{http_code}" -I "${url}"`, (error, stdout) => {
            if (error) {
                resolve(false);
            } else {
                resolve(stdout.trim() === '200');
            }
        });
    });
};

// Read the JSON string from the file
const matrixJson = fs.readFileSync('matrix.json', 'utf8');

// Parse the JSON string into an array of objects
const matrixEntries = JSON.parse(matrixJson);

// Create a new array to store unique entries based on llvm-archive-filename
const seenFilenames = new Set();
const uniqueEntries = [];

(async () => {
    for (const entry of matrixEntries) {
        const filename = entry['llvm-archive-filename'];
        if (!seenFilenames.has(filename)) {
            seenFilenames.add(filename);
            const url = `https://www.mrdocs.com/llvm+clang/${filename}`;
            const exists = await fileExists(url);
            if (!exists) {
                uniqueEntries.push(entry);
            }
        }
    }

    // Convert the new array back to a JSON string
    const uniqueMatrixJson = JSON.stringify(uniqueEntries);

    // Output the filtered JSON string using core.setOutput
    core.setOutput('llvm-matrix', uniqueMatrixJson);

    // Print matrix to console
    console.log(`LLVM Matrix (${uniqueEntries.length} entries):`)
    uniqueEntries.forEach(obj => {
        console.log(`- ${obj.name}`)
        for (const key in obj) {
            if (key !== 'name') {
                console.log(`  ${key}: ${JSON.stringify(obj[key])}`)
            }
        }
    })
})();