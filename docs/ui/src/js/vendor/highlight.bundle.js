;(function () {
  'use strict'

  // The C++ doc-comment colouring and the other language tweaks live in a
  // shared, DOM-free module so the marketing landing page
  // (docs/website/render.js) can register the exact same languages without
  // duplicating the logic. This bundle is just the browser entry point: it
  // builds a highlight.js instance, installs the shared languages, and
  // highlights every code block Antora emitted.
  var hljs = require('highlight.js/lib/highlight')
  require('./mrdocs-highlight-languages.js').register(hljs)

  ;[].slice.call(document.querySelectorAll('pre code.hljs[data-lang]')).forEach(function (node) {
    hljs.highlightBlock(node)
  })
})()
