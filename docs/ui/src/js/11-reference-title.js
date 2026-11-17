;(function () {
  'use strict'

  // Menlo centres each colon in a full-width cell, so "::" in a reference
  // title reads as ": :". Wrap each "::" so doc.css can pull the pair together.
  var title = document.querySelector('.mrdocs-reference .doc > h1.page')
  if (!title) return
  var walker = document.createTreeWalker(title, window.NodeFilter.SHOW_TEXT)
  var nodes = []
  while (walker.nextNode()) nodes.push(walker.currentNode)
  nodes.forEach(function (node) {
    if (node.data.indexOf('::') < 0) return
    var fragment = document.createDocumentFragment()
    node.data.split(/(::)/).forEach(function (part) {
      if (!part) return
      if (part !== '::') return fragment.appendChild(document.createTextNode(part))
      var sep = document.createElement('span')
      sep.className = 'scope-sep'
      sep.textContent = part
      fragment.appendChild(sep)
    })
    node.parentNode.replaceChild(fragment, node)
  })
})()
