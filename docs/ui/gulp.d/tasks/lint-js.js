'use strict'

const eslint = require('gulp-eslint')
const vfs = require('vinyl-fs')

/** Lint JavaScript files.

    This function uses gulp-eslint to lint the specified JavaScript files.
    It creates a stream of files from the specified glob pattern, pipes
    the files to gulp-eslint to lint them, and then formats and prints
    the linting results.

    If any linting errors are found, it emits an error event.

    @param {Array|string} files - The glob pattern(s) of the files to lint.
    @returns {Function} A function that lints the files when called.
*/
function lintJs (files) {
  function lintFiles (done) {
    // Create a stream of files from the specified glob pattern
    return (
      vfs
        .src(files)
        // Pipe the files to gulp-eslint to lint them
        .pipe(eslint())
        // Format and print the linting results
        .pipe(eslint.format())
        // If any linting errors are found, emit an error event
        .pipe(eslint.failAfterError())
        .on('error', done)
    )
  }

  return lintFiles
}

module.exports = lintJs
