;(function () {
  'use strict'
  // Light/dark theme toggle. The initial theme is applied by a small inline
  // script in the page <head> (head-prelude) before first paint to avoid a
  // flash; this handler only flips and persists the choice on click.
  var STORAGE_KEY = 'mrdocs-theme'
  var root = document.documentElement

  function stored () {
    try {
      return window.localStorage.getItem(STORAGE_KEY)
    } catch (e) {
      return null
    }
  }

  function effectiveTheme () {
    var explicit = root.getAttribute('data-theme')
    if (explicit === 'light' || explicit === 'dark') return explicit
    var pref = stored()
    if (pref === 'light' || pref === 'dark') return pref
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
  }

  function apply (theme) {
    root.setAttribute('data-theme', theme)
    try {
      window.localStorage.setItem(STORAGE_KEY, theme)
    } catch (e) {
      /* ignore */
    }
  }

  document.addEventListener('DOMContentLoaded', function () {
    var buttons = document.querySelectorAll('.theme-toggle')
    Array.prototype.forEach.call(buttons, function (btn) {
      btn.addEventListener('click', function () {
        apply(effectiveTheme() === 'dark' ? 'light' : 'dark')
      })
    })
  })
})()
