'use strict'

/** Converts a string to camel case.

    @param {string} name - The string to convert to camel case.
    @returns {string} The string converted to camel case.
 */
function camelCase (name) {
  return name.replace(/[-]./g, (m) => m.slice(1).toUpperCase())
}

/** Exports all modules in the current directory.

    @module index
 */
function exportModules () {
  return require('require-directory')(module, __dirname, { recurse: false, rename: camelCase })
}

module.exports = exportModules()
