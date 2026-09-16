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

  /* The clipped mobile list hides everything but its first few top-level
     entries, so the scroll spy's own target is usually a hidden row and the
     mark lands on nothing -- on generators/reference that is 95 of 104
     positions. Each hidden anchor therefore records the section that governs
     it, and the visible section row carries the mark instead.

     WORTH KNOWING BEFORE YOU TRUST IT: on mobile this is currently correct but
     UNOBSERVABLE. The list sits at the top of the page in normal flow, so
     seeing it means scrolling to the top -- which makes the FIRST section
     active again. A reader can therefore only ever find "Symbols" marked. It
     would light up if the list were ever reachable while scrolled (sticky
     block, or a control in the navbar); a floating contents nav was considered
     for that on 2026-09-16 and deliberately NOT built -- only six of the 3745
     pages carrying a contents list are long enough to want one.

     Kept rather than deleted because the invariant is real: a clipped list
     must not leave the reader unmarked. Delete it only along with the
     clipping. */
  var clippedList = null

  function syncClippedSection () {
    if (!clippedList) return
    var marked = clippedList.querySelector('li.is-section')
    if (marked) marked.classList.remove('is-section')
    var active = clippedList.querySelector('a.is-active')
    if (!active) return
    /* A top-level row governs itself; a clipped child names its section. */
    var href = active.dataset.section || active.getAttribute('href')
    var governing = clippedList.querySelector('a[href="' + href + '"]')
    if (governing && governing.parentNode) governing.parentNode.classList.add('is-section')
  }

  function setActive (fragment, on) {
    var link = links[fragment]
    if (link) link.classList[on ? 'add' : 'remove']('is-active')
    ;(mirrors[fragment] || []).forEach(function (clone) {
      clone.classList[on ? 'add' : 'remove']('is-active')
    })
    syncClippedSection()
  }

  /* Clip the mobile contents list past this many entries, rather than letting
     it run to full length above the page title.

     Measured over the built site: 3745 pages carry a contents list and the
     MEDIAN is 2 entries, so the open block the frame draws (316:334667
     Sidebar, 402x206) is right for almost all of them and must not change.
     Six pages (0.16%) reach this threshold. It is where the list would push
     the title off the first mobile screen: the list is ~88px of chrome plus
     ~30px per entry and the title sits directly below, so the title lands at
     roughly 285 + 88 + 30n, against ~700px of usable viewport on a 402x874
     phone once browser chrome is off -- clear only to n = 10.

     On generators/reference the full list ran 3303px and put the title at
     y3588 against a 285 baseline. */
  var EMBEDDED_CLIP_MIN = 11

  /* How many entries the clipped list shows before "Show all". Top-level only:
     the long pages are shallow and wide -- reference is 9 h2 against 95 h3 --
     so the first few h2s orient the reader, where the first few entries of the
     flat list would just be one section and its children. */
  var EMBEDDED_CLIP_SHOW = 5

  function clipEmbedded (aside, embeddedMenu) {
    var heading = embeddedMenu.querySelector('h3')
    var list = embeddedMenu.querySelector('ul')
    if (!heading || !list) return
    var items = find('li', list)
    if (!items.length) return

    /* The shallowest level present, rather than a hardcoded '1': a page whose
       contents start at an h1.sect0 numbers its top level 0. */
    var top = items.reduce(function (min, li) {
      var level = parseInt(li.dataset.level, 10)
      return level < min ? level : min
    }, Infinity)

    var kept = 0
    var clipped = 0
    var governing = null
    items.forEach(function (li) {
      var anchor = li.querySelector('a')
      if (parseInt(li.dataset.level, 10) === top) {
        /* Governs every row under it, whether or not it survived the clip --
           scrolling into section six must still mark something. */
        governing = anchor
        if (kept < EMBEDDED_CLIP_SHOW) return kept++
      } else if (governing && anchor) {
        anchor.dataset.section = governing.getAttribute('href')
      }
      li.className += (li.className ? ' ' : '') + 'is-extra'
      clipped++
    })
    if (!clipped) return
    clippedList = embeddedMenu

    /* Per-section disclosure. Same reasoning as .nav-item-toggle in the tree:
       the section labels are links, so tapping one navigates rather than
       expanding, which leaves a separate control as the only way to see what a
       section holds without opening all of them.

       It is NOT the tree's left gutter: that reserves 32px on every row to keep
       parents and leaves aligned, which would re-lay out the mobile list the
       frame specifies (316:334668 insets entries 9 past the rule). Right-hand
       placement leaves the alignment alone; the glyph is smaller and muted
       against the panel chevron above it, so the two read as different levels
       rather than the same control twice. */
    find('li', list).forEach(function (li) {
      var anchor = li.querySelector('a')
      if (!anchor || parseInt(li.dataset.level, 10) !== top) return
      var href = anchor.getAttribute('href')
      var children = find('a[data-section="' + href + '"]', list)
      if (!children.length) return
      var toggle = document.createElement('button')
      toggle.type = 'button'
      toggle.className = 'toc-sect-toggle'
      toggle.setAttribute('aria-expanded', 'false')
      toggle.setAttribute('aria-label', 'Show what is in ' + anchor.textContent.trim())
      toggle.addEventListener('click', function (e) {
        e.preventDefault()
        var open = li.classList.toggle('is-open')
        toggle.setAttribute('aria-expanded', String(open))
        children.forEach(function (child) {
          child.parentNode.classList[open ? 'add' : 'remove']('is-shown')
        })
      })
      li.appendChild(toggle)
    })

    /* Same disclosure idiom as the nav drawer's section chips: the chevron
       sits at the far end of the row and the WHOLE row is the hit target, not
       the 16x16 glyph. Unlike .nav-section-toggle this button carries an
       aria-label -- a button whose only content is a ::after glyph has no
       accessible name, so a screen reader would announce it as just "button". */
    var head = document.createElement('div')
    head.className = 'toc-head'
    var toggle = document.createElement('button')
    toggle.type = 'button'
    toggle.className = 'toc-toggle'
    toggle.setAttribute('aria-expanded', 'false')
    toggle.setAttribute('aria-label', 'Show all entries')
    toggle.addEventListener('click', function () {
      var shown = aside.classList.toggle('is-deep')
      toggle.setAttribute('aria-expanded', String(shown))
      toggle.setAttribute('aria-label', shown ? 'Show fewer entries' : 'Show all entries')
    })
    embeddedMenu.insertBefore(head, heading)
    head.appendChild(heading)
    head.appendChild(toggle)
    aside.className += ' toc--long'
  }

  var startOfContent = !document.getElementById('toc') && article.querySelector('h1.page ~ :not(.is-before-toc)')
  if (startOfContent) {
    var embeddedToc = document.createElement('aside')
    embeddedToc.className = 'toc embedded'
    var embeddedMenu = menu.cloneNode(true)
    if (headings.length >= EMBEDDED_CLIP_MIN) clipEmbedded(embeddedToc, embeddedMenu)
    embeddedToc.appendChild(embeddedMenu)
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
