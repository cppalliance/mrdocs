;(function () {
  'use strict'

  // The toolbar is one fixed-height line, so a path that doesn't fit used to
  // wrap out of it. Hide middle crumbs behind a "…" button until the path fits:
  // the first crumb and the current page always stay, the parent goes last.
  // The button shows the whole path, wrapping in a taller bar. It appears
  // whenever the path doesn't fit, even with no middle crumbs to hide, since
  // the current page may still end in an ellipsis. A lone crumb has nothing to
  // hide or reveal, so it just wraps in the taller bar when it doesn't fit.
  var toolbar = document.querySelector('.toolbar')
  var list = toolbar && toolbar.querySelector('.breadcrumbs ul')
  if (!list) return
  var items = Array.prototype.slice.call(list.children)
  if (!items.length) return
  var last = items[items.length - 1]
  var opened = false

  var more = document.createElement('li')
  more.className = 'crumb-more'
  var button = document.createElement('button')
  button.type = 'button'
  button.textContent = '…'
  button.setAttribute('aria-label', 'Show the full path')
  more.appendChild(button)

  // Measured with the current page at its natural width: once it shrinks, its
  // ellipsis hides the overflow from scrollWidth.
  function fits () {
    last.style.flexShrink = '0'
    var ok = list.scrollWidth <= list.clientWidth
    last.style.flexShrink = ''
    return ok
  }

  function layout () {
    if (opened) return
    toolbar.classList.remove('is-expanded')
    items.forEach(function (item) { item.classList.remove('is-hidden') })
    if (more.parentNode) more.parentNode.removeChild(more)
    if (fits()) return
    if (items.length < 2) return toolbar.classList.add('is-expanded')
    items[0].parentNode.insertBefore(more, items[1])
    for (var i = 1; i < items.length - 1 && !fits(); i++) items[i].classList.add('is-hidden')
  }

  // The button disappears once the path is shown, so keyboard focus moves to
  // the first crumb it revealed (or the first crumb) instead of being lost.
  button.addEventListener('click', function () {
    var revealed = items.filter(function (item) { return item.classList.contains('is-hidden') })[0]
    opened = true
    toolbar.classList.add('is-expanded')
    items.forEach(function (item) { item.classList.remove('is-hidden') })
    var target = (revealed || items[0]).querySelector('a')
    if (target) target.focus()
  })

  layout()
  // The crumbs only reach their real width once the web font loads.
  if (document.fonts) document.fonts.ready.then(layout)
  window.addEventListener('resize', layout)
})()
