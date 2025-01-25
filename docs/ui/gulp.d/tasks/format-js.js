'use strict'

const prettier = require('../lib/gulp-prettier-eslint')
const vfs = require('vinyl-fs')

/** Format JavaScript files.

    This function uses gulp-prettier-eslint to format the specified JavaScript files.
    It creates a stream of files from the specified glob pattern, pipes
    the files to gulp-prettier-eslint to format them, and then writes
    the formatted files back to their original location.

    @param {Array|string} files - The glob pattern(s) of the files to format.
    @returns {Function} A function that formats the files when called.
*/
function formatJs (files) {
  function formatFiles () {
    // Create a stream of files from the specified glob pattern
    return (
      vfs
        .src(files)
        // Pipe the files to gulp-prettier-eslint to format them
        .pipe(prettier())
        // Write the formatted files back to their original location
        .pipe(vfs.dest((file) => file.base))
    )
  }

  return formatFiles
}

module.exports = formatJs
