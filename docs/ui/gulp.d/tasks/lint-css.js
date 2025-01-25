'use strict'

const stylelint = require('gulp-stylelint')
const vfs = require('vinyl-fs')

/** Lint CSS files.

    This function uses gulp-stylelint to lint the specified CSS files.

    It creates a stream of files from the specified glob pattern, pipes
    the files to gulp-stylelint to lint them, and then formats and prints
    the linting results.

    If any linting errors are found, it emits an error event.

    @param {Array|string} files - The glob pattern(s) of the files to lint.
    @returns {Function} A function that lints the files when called.
*/
function lintCss (files) {
  function lintFiles (done) {
    // Create a stream of files from the specified glob pattern
    return (
      vfs
        .src(files)
        // Pipe the files to gulp-stylelint to lint them
        .pipe(stylelint({ reporters: [{ formatter: 'string', console: true }], failAfterError: true }))
        // If any linting errors are found, emit an error event
        .on('error', done)
    )
  }

  return lintFiles
}

module.exports = lintCss
