;(function () {
  'use strict'

  /* The hamburger opens the site menu (Figma 316:335019), not the page tree:
     the tree is an inline collapsible section headed by its own chip, and the
     burger stays a hamburger in both of the tree's states. */
  var menu = document.querySelector('.site-menu')
  if (!menu) return

  var toggles = [].slice.call(document.querySelectorAll('.navbar-burger, .nav-toggle'))
  if (!toggles.length) return

  var html = document.documentElement

  function setOpen (open) {
    menu.classList.toggle('is-active', open)
    html.classList.toggle('is-clipped--nav', open)
    toggles.forEach(function (toggle) {
      toggle.classList.toggle('is-active', open)
      if (toggle.hasAttribute('aria-expanded')) toggle.setAttribute('aria-expanded', String(open))
    })
    if (open) html.addEventListener('click', onOutsideClick)
    else html.removeEventListener('click', onOutsideClick)
  }

  function onOutsideClick (e) {
    if (!menu.contains(e.target)) setOpen(false)
  }

  toggles.forEach(function (toggle) {
    toggle.addEventListener('click', function (e) {
      e.stopPropagation()
      setOpen(!menu.classList.contains('is-active'))
    })
  })

  /* Clicks inside stay inside, so the menu does not close before a link is
     followed or the theme toggle fires. */
  menu.addEventListener('click', function (e) {
    e.stopPropagation()
  })

  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape' && menu.classList.contains('is-active')) setOpen(false)
  })

  /* Growing past the breakpoint leaves the desktop navbar showing these
     controls, so the overlay has to close with it. */
  var small = window.matchMedia('(max-width: 1023.5px)')
  small.addEventListener('change', function (e) {
    if (!e.matches) setOpen(false)
  })
})()
