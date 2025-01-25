'use strict'

const stylelint = require('gulp-stylelint')
const vfs = require('vinyl-fs')

/**
    Format CSS files.

    This function uses gulp-prettier-eslint and gulp-stylelint to format and lint the specified CSS files.
    It creates a stream of files from the specified glob pattern, pipes
    the files to gulp-prettier-eslint to format them, then pipes them to gulp-stylelint to lint them,
    and finally writes the formatted and linted files back to their original location.

    @param {Array|string} files - The glob pattern(s) of the files to format.
    @returns {Function} A function that formats the files when called.
*/
function formatCss (files) {
  async function formatFiles () {
    const prettier = (await import('gulp-prettier')).default

    // Create a stream of files from the specified glob pattern
    vfs
      .src(files)
      // Pipe the files to gulp-prettier-eslint to format them
      .pipe(prettier())
      // Pipe the files to gulp-stylelint to lint them
      .pipe(
        stylelint({
          fix: true, // Automatically fix Stylelint issues
          reporters: [{ formatter: 'string', console: true }],
        })
      )
      // Write the formatted and linted files back to their original location
      .pipe(vfs.dest((file) => file.base))
  }

  return formatFiles
}

module.exports = formatCss
