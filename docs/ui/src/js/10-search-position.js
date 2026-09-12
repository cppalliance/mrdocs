;(function () {
  'use strict'

  /* Align the results panel under the search field.

     search.css has to position the panel `fixed` with `right: <gutter>`,
     because the field's own x is not expressible in CSS: it sits in
     `.navbar-brand` under `flex: auto; justify-content: flex-end`, so it lands
     wherever `.navbar-end` leaves it. A right-anchored panel therefore drifts
     away from the field -- measured 139px to its right at both 1440 and 1024,
     with the panel ending 523px past the field's right edge, which reads as a
     tray belonging to the navbar rather than to the input.

     Anchoring needs a measured offset, so it is done here. The panel keeps its
     CSS width and gutter; only `left` is set, clamped so the panel can never
     leave the viewport -- which is the bug the CSS was avoiding in the first
     place. When the clamp bites (narrow viewports, where the panel is nearly
     full width) it lands on the gutter exactly as before.

     If this script does not run the panel simply stays right-anchored, which
     is the previous behaviour and is still fully on-screen. */

  var field = document.getElementById('search-field')
  if (!field || typeof window.MutationObserver === 'undefined') return

  var queued = false

  function place () {
    queued = false
    var panel = field.querySelector('.search-result-dropdown-menu')
    if (!panel) return

    /* Drop our own offsets first, so the panel is back on the CSS gutter and
       we can read it as a USED value. getPropertyValue('--search-panel-gutter')
       would hand back the raw `calc(20 / var(--rem-base) * 1rem)` token, which
       parseFloat turns into NaN -- that read as a 0 gutter and pinned the panel
       flush to the right edge at narrow widths. */
    panel.style.left = ''
    panel.style.right = ''
    var gutter = parseFloat(window.getComputedStyle(panel).right)
    if (isNaN(gutter)) gutter = 0

    var rect = panel.getBoundingClientRect()
    if (!rect.width) return

    var vw = document.documentElement.clientWidth
    var wanted = field.getBoundingClientRect().left
    var furthest = vw - gutter - rect.width

    var left = Math.max(gutter, Math.min(wanted, furthest))
    panel.style.left = left + 'px'
    panel.style.right = 'auto'
  }

  function schedule () {
    if (queued) return
    queued = true
    window.requestAnimationFrame(place)
  }

  // The panel is created and refilled by the search extension, so react to it
  // appearing and to every re-render rather than to input events alone.
  new window.MutationObserver(schedule).observe(field, { childList: true, subtree: true })
  window.addEventListener('resize', schedule)
  schedule()
})()
