// Description: This script reads the matrix.json file and filters
// the main entries for which we test the releases in a clean container.
// The releases are used to generate the demos in multiple environments,
// and one of these environments uploads the generated demos to the
// mrdocs.com server.

const fs = require('fs');
const core = require('@actions/core');
const {exec} = require('child_process');

/**
 * Compares the priority of two compiler entries based on their operating system.
 *
 * @param {Object} entryA - The first entry to compare.
 * @param {string} entryA.os - The operating system of the first entry.
 * @param {string} entryA.compiler - The compiler of the first entry.
 * @param {Object} entryB - The second entry to compare.
 * @param {string} entryB.os - The operating system of the second entry.
 * @param {string} entryB.compiler - The compiler of the second entry.
 * @returns {number} - A negative number if entryA has higher priority,
 *                     a positive number if entryB has higher priority,
 *                     or zero if they have the same priority.
 */
function compareCompilerPriority(entryA, entryB) {
    // Define the compiler priority for each operating system
    const compilerPriority = {
        'windows': ['msvc', 'clang', 'gcc'],
        'macos': ['clang', 'gcc'],
        'linux': ['gcc', 'clang']
    };

    // Retrieve the priority list for the OS of entryA
    const lcOs = entryA.os.toLowerCase();
    if (!compilerPriority.hasOwnProperty(lcOs)) {
        throw new Error(`Unknown operating system: ${entryA.os}`)
    }
    const osPriority = compilerPriority[lcOs]

    // Get the index of the compiler for entryA and entryB in the priority list
    const aPriority = osPriority.indexOf(entryA.compiler)
    const bPriority = osPriority.indexOf(entryB.compiler)

    // If the compiler is not found in the list, assign it the lowest priority
    const aFinalPriority = aPriority === -1 ? osPriority.length : aPriority
    const bFinalPriority = bPriority === -1 ? osPriority.length : bPriority

    // Return the difference between the priorities of entryA and entryB
    return aFinalPriority - bFinalPriority;
}

/**
 * Finds the highest priority entry among all entries that have the same specified value
 * for the `mrdocs-release-package-artifact`.
 *
 * @param {Array<Object>} entries - The array of entries to search.
 * @param {string} artifactName - The value of `mrdocs-release-package-artifact` to match.
 * @returns {Object|null} - The highest priority entry or null if no matching entry is found.
 */
function findHighestPriorityEntry(entries, artifactName) {
    /** @type {Object|null} */
    let highestPriorityEntry = null;

    for (const entry of entries) {
        if (entry['is-main'] !== true) {
            continue;
        }
        if (entry['mrdocs-release-package-artifact'] === artifactName) {
            if (highestPriorityEntry === null) {
                highestPriorityEntry = entry;
            } else {
                if (compareCompilerPriority(entry, highestPriorityEntry) < 0) {
                    highestPriorityEntry = entry;
                }
            }
        }
    }

    return highestPriorityEntry;
}

(async () => {
    // Read the JSON string from the file
    const matrixJson = fs.readFileSync('matrix.json', 'utf8');

    // Parse the JSON string into an array of objects
    const matrixEntries = JSON.parse(matrixJson);

    // Create a new array to store unique entries based on llvm-archive-filename
    const seenArtifactNames = new Set();
    const releaseMatrixEntries = [];

    for (const entry of matrixEntries) {
        if (entry['is-main'] !== true) {
            continue;
        }
        const artifactName = entry['mrdocs-release-package-artifact'];
        if (!seenArtifactNames.has(artifactName)) {
            seenArtifactNames.add(artifactName);
            const highestPriorityEntry = findHighestPriorityEntry(matrixEntries, artifactName);
            if (highestPriorityEntry !== null) {
                releaseMatrixEntries.push(highestPriorityEntry);
            }
        }
    }

    // Convert the new array back to a JSON string
    const uniqueMatrixJson = JSON.stringify(releaseMatrixEntries);

    // Output the filtered JSON string using core.setOutput
    core.setOutput('releases-matrix', uniqueMatrixJson);

    // Print matrix to console
    console.log(`Releases Matrix (${releaseMatrixEntries.length} entries):`)
    releaseMatrixEntries.forEach(obj => {
        console.log(`- ${obj.name}`)
        for (const key in obj) {
            if (key !== 'name') {
                console.log(`  ${key}: ${JSON.stringify(obj[key])}`)
            }
        }
    })
})();
