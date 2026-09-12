;(function () {
  'use strict'

  var SECT_CLASS_RX = /^sect(\d)$/

  var navContainer = document.querySelector('.nav-container')
  if (!navContainer) return
  // The burger belongs to the site menu (08-site-menu.js). This file only
  // handles the page tree, which is an inline section on mobile and the left
  // column on desktop -- no drawer, so no show/hide here.
  // .nav is still needed: scrollItemToMidpoint checks whether the column is
  // sticky before working out the offset.
  var nav = navContainer.querySelector('.nav')
  var navMenuToggle = navContainer.querySelector('.nav-menu-toggle')

  var menuPanel = navContainer.querySelector('[data-panel=menu]')
  if (!menuPanel) return

  var currentPageItem = menuPanel.querySelector('.is-current-page')
  var originalPageItem = currentPageItem
  if (currentPageItem) {
    activateCurrentPath(currentPageItem)
    scrollItemToMidpoint(menuPanel, currentPageItem.querySelector('.nav-link'))
  } else {
    menuPanel.scrollTop = 0
  }

  find(menuPanel, '.nav-item-toggle').forEach(function (btn, idx) {
    var li = btn.parentElement
    // The button ships with no accessible name, so name it after the section
    // it discloses and point it at the list it controls. textContent, not
    // innerHTML: nav labels may carry markup, which must not land in an
    // attribute.
    var label = findNextElement(btn, '.nav-text') || li.querySelector(':scope > .nav-link')
    if (label) btn.setAttribute('aria-label', label.textContent.trim())
    var sublist = li.querySelector(':scope > .nav-list')
    if (sublist) {
      if (!sublist.id) sublist.id = 'nav-sublist-' + idx
      btn.setAttribute('aria-controls', sublist.id)
    }
    btn.addEventListener('click', toggleActive.bind(li))
    var navItemSpan = findNextElement(btn, '.nav-text')
    if (navItemSpan) {
      navItemSpan.style.cursor = 'pointer'
      navItemSpan.addEventListener('click', toggleActive.bind(li))
    }
  })

  syncExpandedState()

  /* The nav is a collapsible section of the page on mobile, headed by the
     component chip, and Figma's default page state (316:335013, 77 tall) has
     it collapsed -- 316:335102 is the same frame expanded to 514. Desktop
     always shows the tree, so the class is applied only below 1024. */
  var sectionToggle = menuPanel.querySelector('.nav-section-toggle')
  var navMenu = menuPanel.querySelector('.nav-menu')
  if (sectionToggle && navMenu) {
    var topList = navMenu.querySelector(':scope > .nav-list')
    if (topList) {
      if (!topList.id) topList.id = 'nav-section-list'
      sectionToggle.setAttribute('aria-controls', topList.id)
    }
    var small = window.matchMedia('(max-width: 1023.5px)')
    var setCollapsed = function (collapsed) {
      navMenu.classList.toggle('is-collapsed', collapsed)
      sectionToggle.setAttribute('aria-expanded', String(!collapsed))
    }
    setCollapsed(small.matches)
    sectionToggle.addEventListener('click', function () {
      setCollapsed(!navMenu.classList.contains('is-collapsed'))
    })
    small.addEventListener('change', function (e) { setCollapsed(e.matches) })
  }

  if (navMenuToggle && menuPanel.querySelector('.nav-item-toggle')) {
    navMenuToggle.style.display = ''
    navMenuToggle.addEventListener('click', function () {
      var collapse = !this.classList.toggle('is-active')
      find(menuPanel, '.nav-item > .nav-item-toggle').forEach(function (btn) {
        collapse ? btn.parentElement.classList.remove('is-active') : btn.parentElement.classList.add('is-active')
      })
      if (currentPageItem) {
        if (collapse) activateCurrentPath(currentPageItem)
        scrollItemToMidpoint(menuPanel, currentPageItem.querySelector('.nav-link'))
      } else {
        menuPanel.scrollTop = 0
      }
      syncExpandedState()
    })
  }

  // NOTE prevent text from being selected by double click
  menuPanel.addEventListener('mousedown', function (e) {
    if (e.detail > 1) e.preventDefault()
  })

  function onHashChange () {
    var navLink
    var hash = window.location.hash
    if (hash) {
      if (hash.indexOf('%')) hash = decodeURIComponent(hash)
      navLink = menuPanel.querySelector('.nav-link[href="' + hash + '"]')
      if (!navLink) {
        var targetNode = document.getElementById(hash.slice(1))
        if (targetNode) {
          var current = targetNode
          var ceiling = document.querySelector('article.doc')
          while ((current = current.parentNode) && current !== ceiling) {
            var id = current.id
            // NOTE: look for section heading
            if (!id && (id = SECT_CLASS_RX.test(current.className))) id = (current.firstElementChild || {}).id
            if (id && (navLink = menuPanel.querySelector('.nav-link[href="#' + id + '"]'))) break
          }
        }
      }
    }
    var navItem
    if (navLink) {
      navItem = navLink.parentNode
    } else if (originalPageItem) {
      navLink = (navItem = originalPageItem).querySelector('.nav-link')
    } else {
      return
    }
    if (navItem === currentPageItem) return
    find(menuPanel, '.nav-item.is-active').forEach(function (el) {
      el.classList.remove('is-active', 'is-current-path', 'is-current-page')
    })
    navItem.classList.add('is-current-page')
    currentPageItem = navItem
    activateCurrentPath(navItem)
    scrollItemToMidpoint(menuPanel, navLink)
    syncExpandedState()
  }

  if (menuPanel.querySelector('.nav-link[href^="#"]')) {
    if (window.location.hash) onHashChange()
    window.addEventListener('hashchange', onHashChange)
  }

  function activateCurrentPath (navItem) {
    var ancestorClasses
    var ancestor = navItem.parentNode
    while (!(ancestorClasses = ancestor.classList).contains('nav-menu')) {
      if (ancestor.tagName === 'LI' && ancestorClasses.contains('nav-item')) {
        ancestorClasses.add('is-active', 'is-current-path')
      }
      ancestor = ancestor.parentNode
    }
    navItem.classList.add('is-active')
  }

  /* Mirrors the is-active class onto aria-expanded. The class is set from
     four places (here, activateCurrentPath, the expand-all toggle and
     onHashChange), so every caller re-syncs the whole tree rather than each
     one remembering to update its own button. Six toggles, so the walk is
     cheaper than the bookkeeping. */
  function syncExpandedState () {
    find(menuPanel, '.nav-item > .nav-item-toggle').forEach(function (btn) {
      btn.setAttribute('aria-expanded', btn.parentElement.classList.contains('is-active'))
    })
  }

  function toggleActive () {
    if (this.classList.toggle('is-active')) {
      var padding = parseFloat(window.getComputedStyle(this).marginTop)
      var rect = this.getBoundingClientRect()
      var menuPanelRect = menuPanel.getBoundingClientRect()
      var overflowY = (rect.bottom - menuPanelRect.top - menuPanelRect.height + padding).toFixed()
      if (overflowY > 0) menuPanel.scrollTop += Math.min((rect.top - menuPanelRect.top - padding).toFixed(), overflowY)
    }
    syncExpandedState()
  }

  function scrollItemToMidpoint (panel, el) {
    var rect = panel.getBoundingClientRect()
    var effectiveHeight = rect.height
    var navStyle = window.getComputedStyle(nav)
    if (navStyle.position === 'sticky') effectiveHeight -= rect.top - parseFloat(navStyle.top)
    panel.scrollTop = Math.max(0, (el.getBoundingClientRect().height - effectiveHeight) * 0.5 + el.offsetTop)
  }

  function find (from, selector) {
    return [].slice.call(from.querySelectorAll(selector))
  }

  function findNextElement (from, selector) {
    var el = from.nextElementSibling
    return el && selector ? el[el.matches ? 'matches' : 'msMatchesSelector'](selector) && el : el
  }
})()
