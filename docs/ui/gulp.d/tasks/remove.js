'use strict'

const fs = require('fs-extra')
const { Transform } = require('stream')
const vfs = require('vinyl-fs')

/** Creates a transform stream that applies a given function to each file.

    @param {Function} transformFunction - The function to be applied to each file.
    @returns {Transform} A transform stream.
 */
function createTransformStream (transformFunction) {
  return new Transform({ objectMode: true, transform: transformFunction })
}

/** Removes a file.

    @param {Object} file - The file to be removed.
    @param {string} enc - The encoding used.
    @param {Function} next - The callback function to be called after the file is removed.
 */
function removeFile (file, enc, next) {
  fs.remove(file.path, next)
}

/** Creates a stream of files and applies a given function to each file.

    @param {Array} files - The files to be processed.
    @param {Function} processFunction - The function to be applied to each file.
    @returns {Stream} A stream of files.
 */
function processFiles (files, processFunction) {
  return vfs.src(files, { allowEmpty: true }).pipe(processFunction)
}

/** Exports a function that removes given files.

    @param {Array} files - The files to be removed.
    @returns {Function} A function that removes the given files when called.
 */
function removeFiles (files) {
  function removeFilesFunction () {
    return processFiles(files, createTransformStream(removeFile))
  }

  return removeFilesFunction
}

module.exports = removeFiles
