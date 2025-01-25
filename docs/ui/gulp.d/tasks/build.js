'use strict'

const autoprefixer = require('autoprefixer')
const browserify = require('browserify')
const concat = require('gulp-concat')
const cssnano = require('cssnano')
const fs = require('fs-extra')
const imagemin = require('gulp-imagemin')
const merge = require('merge-stream')
const ospath = require('path')
const path = ospath.posix
const postcss = require('gulp-postcss')
const postcssCalc = require('postcss-calc')
const postcssImport = require('postcss-import')
const postcssUrl = require('postcss-url')
const postcssVar = require('postcss-custom-properties')
const { Transform } = require('stream')
const map = (transform) => new Transform({ objectMode: true, transform })
const through = () => map((file, enc, next) => next(null, file))
const uglify = require('gulp-uglify')
const vfs = require('vinyl-fs')

/** Bundles JavaScript files using Browserify.

    This function takes an object as a parameter with the base directory and
    the bundle extension. It returns a map function that processes each file.
    If the file ends with the bundle extension, it uses Browserify to bundle the file.
    It also updates the modification time of the file if any of its dependencies
    have a newer modification time.

    @param {Object} options - The options for bundling.
    @param {string} options.base - The base directory.
    @param {string} options.ext - The bundle extension.
    @returns {Function} A function that processes each file.
 */
function bundleJsFiles ({ base: basedir, ext: bundleExt = '.bundle.js' }) {
  // Return a map function that processes each file
  return map((file, enc, next) => {
    // If the file ends with the bundle extension, bundle it
    if (bundleExt && file.relative.endsWith(bundleExt)) {
      const mtimePromises = []
      const bundlePath = file.path

      // Use Browserify to bundle the file
      browserify(file.relative, { basedir, detectGlobals: false })
        .plugin('browser-pack-flat/plugin')
        .on('file', (bundledPath) => {
          // If the bundled file is not the original file, add its modification time to the promises
          if (bundledPath !== bundlePath) mtimePromises.push(fs.stat(bundledPath).then(({ mtime }) => mtime))
        })
        .bundle((bundleError, bundleBuffer) =>
          // When all modification times are available, update the file's modification time if necessary
          Promise.all(mtimePromises).then((mtimes) => {
            const newestMtime = mtimes.reduce((max, curr) => (curr > max ? curr : max), file.stat.mtime)
            if (newestMtime > file.stat.mtime) file.stat.mtimeMs = +(file.stat.mtime = newestMtime)
            if (bundleBuffer !== undefined) file.contents = bundleBuffer
            next(bundleError, Object.assign(file, { path: file.path.slice(0, file.path.length - 10) + '.js' }))
          })
        )
      return
    }
    // If the file does not end with the bundle extension, just read its contents
    fs.readFile(file.path, 'UTF-8').then((contents) => {
      next(null, Object.assign(file, { contents: Buffer.from(contents) }))
    })
  })
}

/** Fixes pseudo elements in CSS.

    This function walks through the CSS rules and replaces single colons with double colons
    for pseudo elements. It uses a regular expression to match the pseudo elements and then
    replaces the single colon with a double colon.

    @param {Object} css - The PostCSS CSS object.
    @param {Object} result - The result object.
 */
function postcssPseudoElementFixer (css, result) {
  // Walk through the CSS rules
  css.walkRules(/(?:^|[^:]):(?:before|after)/, (rule) => {
    // For each rule, replace single colon with double colon for pseudo elements
    rule.selector = rule.selectors.map((it) => it.replace(/(^|[^:]):(before|after)$/, '$1::$2')).join(',')
  })
}
/** Returns an array of PostCSS plugins.

    This function takes the destination directory and a boolean
    indicating whether to generate a preview as parameters.

    It returns an array of PostCSS plugins to be used for
    processing CSS files.

    @param {string} dest - The destination directory.
    @param {boolean} preview - Whether to generate a preview.
    @returns {Array} An array of PostCSS plugins.
 */
function getPostCssPlugins (dest, preview) {
  // Return an array of PostCSS plugins
  return [
    // Import CSS files
    postcssImport,
    // Custom plugin to update the modification time of the file
    (css, { messages, opts: { file } }) =>
      Promise.all(
        messages
          .reduce((accum, { file: depPath, type }) => (type === 'dependency' ? accum.concat(depPath) : accum), [])
          .map((importedPath) => fs.stat(importedPath).then(({ mtime }) => mtime))
      ).then((mtimes) => {
        const newestMtime = mtimes.reduce((max, curr) => (!max || curr > max ? curr : max), file.stat.mtime)
        if (newestMtime > file.stat.mtime) file.stat.mtimeMs = +(file.stat.mtime = newestMtime)
      }),
    // Resolve URLs in CSS files
    postcssUrl([
      {
        filter: new RegExp('^src/css/[~][^/]*(?:font|face)[^/]*/.*/files/.+[.](?:ttf|woff2?)$'),
        url: (asset) => {
          const relpath = asset.pathname.slice(1)
          const abspath = require.resolve(relpath)
          const basename = ospath.basename(abspath)
          const destpath = ospath.join(dest, 'font', basename)
          if (!fs.pathExistsSync(destpath)) fs.copySync(abspath, destpath)
          return path.join('..', 'font', basename)
        },
      },
    ]),
    // Process CSS custom properties
    postcssVar({ preserve: preview }),
    // Calculate CSS values (only in preview mode)
    preview ? postcssCalc : () => {}, // cssnano already applies postcssCalc
    // Add vendor prefixes to CSS rules
    autoprefixer,
    // Minify CSS (only in non-preview mode)
    preview
      ? () => {}
      : (css, result) => cssnano({ preset: 'default' })(css, result).then(() => postcssPseudoElementFixer(css, result)),
  ]
}

/** Returns an array of tasks for building the UI.

    This function takes several parameters including options for the vfs source,
    whether to generate source maps, PostCSS plugins to use, whether to generate a preview,
    and the source directory. It returns an array of tasks that are used to build the UI.

    @param {Object} opts - The options for the vfs source.
    @param {boolean} sourcemaps - Whether to generate source maps.
    @param {Array} postcssPlugins - The PostCSS plugins to use.
    @param {boolean} preview - Whether to generate a preview.
    @param {string} src - The source directory.
    @returns {Array} An array of tasks for building the UI.
 */
function getAllTasks (opts, sourcemaps, postcssPlugins, preview, src) {
  // Return an array of tasks
  return [
    // Task for getting the 'ui.yml' file
    vfs.src('ui.yml', { ...opts, allowEmpty: true }),
    // Task for bundling JavaScript files
    vfs
      .src('js/+([0-9])-*.js', { ...opts, read: false, sourcemaps })
      .pipe(bundleJsFiles(opts))
      .pipe(uglify({ output: { comments: /^! / } }))
      // NOTE concat already uses stat from newest combined file
      .pipe(concat('js/site.js')),
    // Task for bundling vendor JavaScript files
    vfs
      .src('js/vendor/*([^.])?(.bundle).js', { ...opts, read: false })
      .pipe(bundleJsFiles(opts))
      .pipe(uglify({ output: { comments: /^! / } })),
    // Task for getting vendor minified JavaScript files
    vfs
      .src('js/vendor/*.min.js', opts)
      .pipe(map((file, enc, next) => next(null, Object.assign(file, { extname: '' }, { extname: '.js' })))),
    vfs
      .src(['css/site.css', 'css/vendor/*.css'], { ...opts, sourcemaps })
      .pipe(postcss((file) => ({ plugins: postcssPlugins, options: { file } }))),
    // Task for getting font files
    vfs.src('font/*.{ttf,woff*(2)}', opts),
    // Task for getting image files
    vfs.src('img/**/*.{gif,ico,jpg,png,svg}', opts).pipe(
      preview
        ? through()
        : imagemin(
          [
            imagemin.gifsicle(),
            imagemin.jpegtran(),
            // imagemin.optipng(),
            imagemin.svgo({
              plugins: [
                { cleanupIDs: { preservePrefixes: ['icon-', 'view-'] } },
                { removeViewBox: false },
                { removeDesc: false },
              ],
            }),
          ].reduce((accum, it) => (it ? accum.concat(it) : accum), [])
        )
    ),
    // Task for getting helper JavaScript files
    vfs.src('helpers/*.js', opts),
    // Task for getting layout handlebars files
    vfs.src('layouts/*.hbs', opts),
    // Task for getting partial handlebars files
    vfs.src('partials/*.hbs', opts),
    // Task for getting static files
    vfs.src('static/**/*[!~]', { ...opts, base: ospath.join(src, 'static'), dot: true }),
  ]
}

/**
    Builds UI assets.

    This function takes the source directory, destination directory, and a boolean
    indicating whether to generate a preview as parameters.

    @param {string} src - The source directory.
    @param {string} dest - The destination directory.
    @param {boolean} preview - Whether to generate a preview.
    @returns {Function} The `UiAssetsBuilder` function.
 */
function buildUiAssets (src, dest, preview) {
  // Define an async function `UiAssetsBuilder`
  async function UiAssetsBuilder () {
    // Prepare the options
    const opts = { base: src, cwd: src }
    const sourcemaps = preview || process.env.SOURCEMAPS === 'true'

    // Get the PostCSS plugins
    const postcssPlugins = getPostCssPlugins(dest, preview)

    // Get all tasks
    const tasks = getAllTasks(opts, sourcemaps, postcssPlugins, preview, src)

    // Merge all tasks and pipe them to the destination directory
    // Return a Promise that resolves when the stream ends and
    // rejects if an error occurs in the stream
    return new Promise((resolve, reject) => {
      merge(...tasks)
        .pipe(vfs.dest(dest, { sourcemaps: sourcemaps && '.' }))
        .on('end', resolve) // Resolve the Promise when the stream ends
        .on('error', reject) // Reject the Promise if an error occurs in the stream
    })
  }

  // Return the `UiAssetsBuilder` function
  return UiAssetsBuilder
}
module.exports = buildUiAssets
