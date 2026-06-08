;(function () {
  'use strict'

  var article = document.querySelector('article.doc')
  if (!article) return
  var navbar = document.querySelector('nav.navbar')
  var supportsScrollToOptions = 'scrollTo' in document.documentElement

  // Returns the height of an element only when it's actually pinned to the
  // top of the viewport (fixed or sticky). A statically-positioned element
  // scrolls away with the content and shouldn't reserve space at the top.
  function pinnedHeight (el) {
    if (!el) return 0
    var pos = window.getComputedStyle(el).position
    return (pos === 'fixed' || pos === 'sticky') ? el.offsetHeight : 0
  }

  function decodeFragment (hash) {
    return hash && (~hash.indexOf('%') ? decodeURIComponent(hash) : hash).slice(1)
  }

  function computePosition (el, sum) {
    return article.contains(el) ? computePosition(el.offsetParent, el.offsetTop + sum) : sum
  }

  function jumpToAnchor (e) {
    if (e) {
      if (e.altKey || e.ctrlKey) return
      window.location.hash = '#' + this.id
      e.preventDefault()
    }
    var elementTop = computePosition(this, 0)
    // Only subtract heights of bars that remain pinned to the top of the
    // viewport after scrolling. The site navbar is `position: fixed`, so
    // its height is the offset we need; the breadcrumb `.toolbar` is
    // `position: static` and scrolls away, so it should not be subtracted.
    // Add a small breathing-room gap so the heading is not glued to the
    // navbar's bottom edge.
    var ANCHOR_BREATHING_ROOM = 18
    var y = elementTop - pinnedHeight(navbar) - ANCHOR_BREATHING_ROOM
    var instant = e === false && supportsScrollToOptions
    instant ? window.scrollTo({ left: 0, top: y, behavior: 'instant' }) : window.scrollTo(0, y)

    updateTocHighlighting('#' + this.id)
  }

  function updateTocHighlighting (targetFragment) {
    var tocLinks = document.querySelectorAll('aside.toc a[href^="#"]')
    if (tocLinks.length === 0) return

    tocLinks.forEach(function (link) {
      link.classList.remove('is-active')
    })

    var targetLink = document.querySelector('aside.toc a[href="' + targetFragment + '"]')
    if (targetLink) {
      targetLink.classList.add('is-active')
    }
  }

  window.addEventListener('load', function jumpOnLoad (e) {
    var fragment, target
    if ((fragment = decodeFragment(window.location.hash)) && (target = document.getElementById(fragment))) {
      jumpToAnchor.call(target, false)
      setTimeout(jumpToAnchor.bind(target, false), 250)
    }
    window.removeEventListener('load', jumpOnLoad)
  })

  Array.prototype.slice.call(document.querySelectorAll('a[href^="#"]')).forEach(function (el) {
    var fragment, target
    if ((fragment = decodeFragment(el.hash)) && (target = document.getElementById(fragment))) {
      el.addEventListener('click', jumpToAnchor.bind(target))
    }
  })
})()
