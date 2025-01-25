'use strict'

const Asciidoctor = require('@asciidoctor/core')()
const fs = require('fs-extra')
const handlebars = require('handlebars')
const merge = require('merge-stream')
const ospath = require('path')
const path = ospath.posix
const requireFromString = require('require-from-string')
const { Transform } = require('stream')
const map = (transform = () => {}, flush = undefined) => new Transform({ objectMode: true, transform, flush })
const vfs = require('vinyl-fs')
const yaml = require('js-yaml')

const ASCIIDOC_ATTRIBUTES = {
  experimental: '',
  icons: 'font',
  sectanchors: '',
  'source-highlighter': 'highlight.js',
}

/** Load the sample UI model from a YAML file.

 This function reads the 'ui-model.yml' file from the
 specified source directory and parses its contents as YAML.

 @param {string} src - The source directory.
 @returns {Promise} A promise that resolves with the parsed YAML data.
 */
function loadSampleUiModel (src) {
  return fs.readFile(ospath.join(src, 'ui-model.yml'), 'utf8').then((contents) => yaml.safeLoad(contents))
}

/** Register Handlebars partials.

 This function reads Handlebars partial files from the specified source directory
 and registers them as partials.

 @param {string} src - The source directory.
 @returns {Stream} A stream that ends when all partials have been registered.
 */
function registerPartials (src) {
  return vfs.src('partials/*.hbs', { base: src, cwd: src }).pipe(
    map((file, enc, next) => {
      handlebars.registerPartial(file.stem, file.contents.toString())
      next()
    })
  )
}

/** Register Handlebars helpers.

 This function reads JavaScript files from the specified source directory
 and registers them as Handlebars helpers.

 @param {string} src - The source directory.
 @returns {Stream} A stream that ends when all helpers have been registered.
 */
function registerHelpers (src) {
  handlebars.registerHelper('resolvePage', resolvePage)
  handlebars.registerHelper('resolvePageURL', resolvePageURL)
  return vfs.src('helpers/*.js', { base: src, cwd: src }).pipe(
    map((file, enc, next) => {
      handlebars.registerHelper(file.stem, requireFromString(file.contents.toString()))
      next()
    })
  )
}

/** Compile Handlebars layouts.

 This function reads Handlebars layout files from the specified source directory
 and compiles them.

 @param {string} src - The source directory.
 @returns {Stream} A stream that ends when all layouts have been compiled.
 */
function compileLayouts (src) {
  const layouts = new Map()
  return vfs.src('layouts/*.hbs', { base: src, cwd: src }).pipe(
    map(
      (file, enc, next) => {
        const srcName = path.join(src, file.relative)
        layouts.set(file.stem, handlebars.compile(file.contents.toString(), { preventIndent: true, srcName }))
        next()
      },
      function (done) {
        this.push({ layouts })
        done()
      }
    )
  )
}

/** Copy images.

 This function copies image files from the specified source directory
 to the specified destination directory.

 @param {string} src - The source directory.
 @param {string} dest - The destination directory.
 @returns {Stream} A stream that ends when all images have been copied.
 */
function copyImages (src, dest) {
  return vfs
    .src('**/*.{png,svg}', { base: src, cwd: src })
    .pipe(vfs.dest(dest))
    .pipe(map((file, enc, next) => next()))
}

/** Resolve a page.

 This function resolves a page specification to a URL.

 @param {string} spec - The page specification.
 @param {Object} context - The context (not used).
 @returns {Object} An object with a 'pub' property containing the resolved URL.
 */
function resolvePage (spec, context = {}) {
  if (spec) return { pub: { url: resolvePageURL(spec) } }
}

/** Resolve a page URL.

 This function resolves a page specification to a URL.

 @param {string} spec - The page specification.
 @param {Object} context - The context (not used).
 @returns {string} The resolved URL.
 */
function resolvePageURL (spec, context = {}) {
  if (spec) return '/' + (spec = spec.split(':').pop()).slice(0, spec.lastIndexOf('.')) + '.html'
}

/** Transform a Handlebars error.

 This function transforms a Handlebars error into a more readable format.

 @param {Object} error - The error object.
 @param {string} layout - The layout that caused the error.
 @returns {Error} The transformed error.
 */
function transformHandlebarsError ({ message, stack }, layout) {
  const m = stack.match(/^ *at Object\.ret \[as (.+?)\]/m)
  const templatePath = `src/${m ? 'partials/' + m[1] : 'layouts/' + layout}.hbs`
  const err = new Error(`${message}${~message.indexOf('\n') ? '\n^ ' : ' '}in UI template ${templatePath}`)
  err.stack = [err.toString()].concat(stack.slice(message.length + 8)).join('\n')
  return err
}

/** Convert a stream to a promise.

 This function converts a stream to a promise that resolves when the stream ends.

 @param {Stream} stream - The stream.
 @returns {Promise} A promise that resolves when the stream ends.
 */
function toPromise (stream) {
  return new Promise((resolve, reject, data = {}) =>
    stream
      .on('error', reject)
      .on('data', (chunk) => chunk.constructor === Object && Object.assign(data, chunk))
      .on('finish', () => resolve(data))
  )
}

/** Process base UI model.

 This function processes the base UI model and returns it along with the layouts.

 @param {Object} baseUiModel - The base UI model.
 @param {Object} layouts - The layouts.
 @returns {Array} An array containing the processed base UI model and the layouts.
 */
function processBaseUiModel (baseUiModel) {
  // Register Asciidoctor extensions defined in baseUiModel.asciidoc.extensions
  const extensions = ((baseUiModel.asciidoc || {}).extensions || []).map((extensionRequest) => {
    ASCIIDOC_ATTRIBUTES[extensionRequest.replace(/^@|\.js$/, '').replace(/[/]/g, '-') + '-loaded'] = ''
    const extension = require(extensionRequest)
    extension.register.call(Asciidoctor.Extensions)
    return extension
  })
  const asciidoc = { extensions }
  for (const component of baseUiModel.site.components) {
    for (const version of component.versions || []) version.asciidoc = asciidoc
  }
  // Add environment variables to the base UI model
  baseUiModel = { ...baseUiModel, env: process.env }
  delete baseUiModel.asciidoc
}

/** Build pages.

    This function builds the pages and writes the converted files back
    to their original location.

    For each .adoc file in preview-src directory, it reads the file,
    and converts it to HTML using Asciidoctor,

    @param {string} previewSrc - The preview source directory.
    @param {Object} baseUiModel - The base UI model.
    @param {Object} layouts - The layouts.
    @param {string} previewDest - The preview destination directory.
    @param {Function} done - The done callback.
    @param {Function} sink - The sink function.
 */
function buildAsciidocPages (previewSrc, baseUiModel, layouts, previewDest, done, sink) {
  return vfs
    .src('**/*.adoc', { base: previewSrc, cwd: previewSrc })
    .pipe(
      map((file, enc, next) => {
        // siteRootPath is relative to the file being processed
        const siteRootPath = path.relative(ospath.dirname(file.path), ospath.resolve(previewSrc))
        const uiModel = { ...baseUiModel }
        uiModel.page = { ...uiModel.page }
        uiModel.siteRootPath = siteRootPath
        uiModel.uiRootPath = path.join(siteRootPath, '_')
        if (file.stem === '404') {
          uiModel.page = { layout: '404', title: 'Page Not Found' }
        } else {
          const asciiDocAttributes = { ...ASCIIDOC_ATTRIBUTES }
          if (process.argv.includes('--remove-sidenav')) {
            asciiDocAttributes['page-remove-sidenav'] = '@'
          }
          if (process.argv.includes('--page-toc')) {
            asciiDocAttributes['page-page-toc'] = '@'
          }
          const doc = Asciidoctor.load(file.contents, { safe: 'safe', attributes: asciiDocAttributes })
          uiModel.page.attributes = Object.entries(doc.getAttributes())
            .filter(([name, val]) => name.startsWith('page-'))
            .reduce((accum, [name, val]) => {
              accum[name.slice(5)] = val
              return accum
            }, {})
          uiModel.page.layout = doc.getAttribute('page-layout', 'default')
          uiModel.page.title = doc.getDocumentTitle()
          uiModel.page.contents = Buffer.from(doc.convert())
        }
        file.extname = '.html'
        try {
          file.contents = Buffer.from(layouts.get(uiModel.page.layout)(uiModel))
          next(null, file)
        } catch (e) {
          next(transformHandlebarsError(e, uiModel.page.layout))
        }
      })
    )
    .pipe(vfs.dest(previewDest))
    .on('error', done)
    .pipe(sink())
}

/** Build preview pages.

 This function uses Asciidoctor to convert AsciiDoc files to HTML for preview.
 It creates a stream of AsciiDoc files from the specified glob pattern, converts
 the files to HTML using Asciidoctor, and then writes the converted files back
 to their original location.

 @param {string} src - The source directory.
 @param {string} previewSrc - The preview source directory.
 @param {string} previewDest - The preview destination directory.
 @param {Function} sink - The sink function.
 @returns {Function} A function that builds the preview pages when called.
 */
function buildPreviewPages (src, previewSrc, previewDest, sink = () => map()) {
  async function buildPages (done) {
    try {
      // Use Promise.all to run multiple basic tasks in parallel
      const [baseUiModel, { layouts }] = await Promise.all([
        // Load the sample UI model from a YAML file
        loadSampleUiModel(previewSrc),
        // Merge multiple streams into one and convert it to a promise
        // The streams are created by compiling layouts, registering partials
        // and helpers, and copying images
        toPromise(
          merge(compileLayouts(src), registerPartials(src), registerHelpers(src), copyImages(previewSrc, previewDest))
        ),
      ])

      // Process the base UI model and get the processed base UI model and layouts
      processBaseUiModel(baseUiModel)

      // Build the AsciiDoc pages and write the converted files back to their original location
      await buildAsciidocPages(previewSrc, baseUiModel, layouts, previewDest, done, sink)
    } catch (error) {
      // If an error occurs during the execution of the promises, it will be caught here
      done(error)
    }
  }

  return buildPages
}

module.exports = buildPreviewPages
