'use strict'

const vfs = require('vinyl-fs')
const zip = require('gulp-vinyl-zip')
const path = require('path')

/** Creates a zip file from the specified source directory.

    This function uses gulp-vinyl-zip to create a zip file from the specified source directory.
    The zip file is saved in the specified destination directory with a name based on the provided bundle name.
    When the zip file is created, it calls the onFinish callback with the path of the zip file.

    @param {string} src - The source directory.
    @param {string} dest - The destination directory.
    @param {string} bundleName - The name of the bundle.
    @param {Function} onFinish - The callback function to call when the zip file is created.

    @returns {Function} A function that creates the zip file when called.
 */
function pack (src, dest, bundleName, onFinish) {
  function createZipFile () {
    // Create a stream of files from the source directory
    return (
      vfs
        .src('**/*', { base: src, cwd: src })
        // Pipe the files to gulp-vinyl-zip to create a zip file
        .pipe(zip.dest(path.join(dest, `${bundleName}-bundle.zip`)))
        // When the zip file is created, call the onFinish callback with the path of the zip file
        .on('finish', () => onFinish && onFinish(path.resolve(dest, `${bundleName}-bundle.zip`)))
    )
  }

  return createZipFile
}

module.exports = pack
