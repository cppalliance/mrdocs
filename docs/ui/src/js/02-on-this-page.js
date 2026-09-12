;(function () {
  'use strict'

  var sidebar = document.querySelector('aside.toc.sidebar')
  if (!sidebar) return
  if (document.querySelector('body.-toc')) return sidebar.parentNode.removeChild(sidebar)
  var levels = parseInt(sidebar.dataset.levels || 2, 10)
  if (levels < 0) return

  var articleSelector = 'article.doc'
  var article = document.querySelector(articleSelector)
  if (!article) return
  var headingsSelector = []
  for (var level = 0; level <= levels; level++) {
    var headingSelector = [articleSelector]
    if (level) {
      for (var l = 1; l <= level; l++) headingSelector.push((l === 2 ? '.sectionbody>' : '') + '.sect' + l)
      headingSelector.push('h' + (level + 1) + '[id]' + (level > 1 ? ':not(.discrete)' : ''))
    } else {
      headingSelector.push('h1[id].sect0')
    }
    headingsSelector.push(headingSelector.join('>'))
  }
  var headings = find(headingsSelector.join(','), article.parentNode)
  if (!headings.length) return sidebar.parentNode.removeChild(sidebar)

  var lastActiveFragment
  var links = {}
  var list = headings.reduce(function (accum, heading) {
    var link = document.createElement('a')
    link.textContent = heading.textContent
    links[(link.href = '#' + heading.id)] = link
    var listItem = document.createElement('li')
    listItem.dataset.level = parseInt(heading.nodeName.slice(1), 10) - 1
    listItem.appendChild(link)
    accum.appendChild(listItem)
    return accum
  }, document.createElement('ul'))

  var menu = sidebar.querySelector('.toc-menu')
  if (!menu) (menu = document.createElement('div')).className = 'toc-menu'

  var title = document.createElement('h3')
  title.textContent = sidebar.dataset.title || 'Contents'
  menu.appendChild(title)
  menu.appendChild(list)

  /* Each copy of the list carries its own anchors, so `links` -- which holds
     the sidebar's, and is what the scroll maths measures -- is not enough to
     mark the active entry everywhere. Clones register here and setActive
     mirrors the class onto them. Before this the embedded copy never
     highlighted at all. */
  var mirrors = {}

  function registerMirror (container) {
    find('a[href^="#"]', container).forEach(function (link) {
      (mirrors[link.hash] = mirrors[link.hash] || []).push(link)
    })
  }

  function setActive (fragment, on) {
    var link = links[fragment]
    if (link) link.classList[on ? 'add' : 'remove']('is-active')
    ;(mirrors[fragment] || []).forEach(function (clone) {
      clone.classList[on ? 'add' : 'remove']('is-active')
    })
  }

  var startOfContent = !document.getElementById('toc') && article.querySelector('h1.page ~ :not(.is-before-toc)')
  if (startOfContent) {
    var embeddedToc = document.createElement('aside')
    embeddedToc.className = 'toc embedded'
    embeddedToc.appendChild(menu.cloneNode(true))
    /* Figma 316:334667 stacks the contents list ABOVE the article body -- the
       Sidebar sits at y=183 and the content block starts at y=635 -- so it
       goes before the page title, not after it. main.css hides this copy from
       1024 up, so the placement only shows on mobile.

       It goes BEFORE article.doc rather than inside it, because the content
       band is painted as a background on .doc: as a child it sat inside that
       background box, so the band's clouds ran behind the contents list
       instead of starting at the title. The 402 frame puts the list on the
       plain page ground and begins the band at the h1. The list full-bleeds
       itself (margin-inline: calc(50% - 50vw)), so it does not depend on
       .doc's own inset. */
    if (article.parentNode) article.parentNode.insertBefore(embeddedToc, article)
    else startOfContent.parentNode.insertBefore(embeddedToc, startOfContent)
    registerMirror(embeddedToc)
  }

  window.addEventListener('load', function () {
    onScroll()
    window.addEventListener('scroll', onScroll)
  })

  function onScroll () {
    var scrolledBy = window.pageYOffset
    var buffer = getNumericStyleVal(document.documentElement, 'fontSize') * 1.15
    var ceil = article.offsetTop
    if (scrolledBy && window.innerHeight + scrolledBy + 2 >= document.documentElement.scrollHeight) {
      lastActiveFragment = Array.isArray(lastActiveFragment) ? lastActiveFragment : Array(lastActiveFragment || 0)
      var activeFragments = []
      var lastIdx = headings.length - 1
      headings.forEach(function (heading, idx) {
        var fragment = '#' + heading.id
        if (idx === lastIdx || heading.getBoundingClientRect().top + getNumericStyleVal(heading, 'paddingTop') > ceil) {
          activeFragments.push(fragment)
          if (lastActiveFragment.indexOf(fragment) < 0) setActive(fragment, true)
        } else if (~lastActiveFragment.indexOf(fragment)) {
          setActive(lastActiveFragment.shift(), false)
        }
      })
      list.scrollTop = list.scrollHeight - list.offsetHeight
      lastActiveFragment = activeFragments.length > 1 ? activeFragments : activeFragments[0]
      return
    }
    if (Array.isArray(lastActiveFragment)) {
      lastActiveFragment.forEach(function (fragment) {
        setActive(fragment, false)
      })
      lastActiveFragment = undefined
    }
    var activeFragment
    headings.some(function (heading) {
      if (heading.getBoundingClientRect().top + getNumericStyleVal(heading, 'paddingTop') - buffer > ceil) return true
      activeFragment = '#' + heading.id
    })
    /* Above the first heading nothing matches, which left the whole contents
       list unmarked -- and since the gold rule only paints on the active
       entry (Figma 316:335140 is the one link whose stroke is at full
       opacity; the rest sit at 0), the list showed no rule at all until you
       scrolled. Fall back to the first heading so one entry is always
       marked. */
    if (!activeFragment && headings.length) activeFragment = '#' + headings[0].id
    if (activeFragment) {
      if (activeFragment === lastActiveFragment) return
      if (lastActiveFragment) setActive(lastActiveFragment, false)
      var activeLink = links[activeFragment]
      setActive(activeFragment, true)
      if (list.scrollHeight > list.offsetHeight) {
        list.scrollTop = Math.max(0, activeLink.offsetTop + activeLink.offsetHeight - list.offsetHeight)
      }
      lastActiveFragment = activeFragment
    } else if (lastActiveFragment) {
      setActive(lastActiveFragment, false)
      lastActiveFragment = undefined
    }
  }

  function find (selector, from) {
    return [].slice.call((from || document).querySelectorAll(selector))
  }

  function getNumericStyleVal (el, prop) {
    return parseFloat(window.getComputedStyle(el)[prop])
  }
})()
