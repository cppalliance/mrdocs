;(function () {
  'use strict'

  /* The search field is unusable for a moment after every page load, which
     reads as a broken control.

     Why: the lunr index is ~6MB -- 3408 of its 3529 documents are generated
     C++ reference pages -- and search-ui.js disables the field until that
     file arrives, setting title="Loading index...". It also attaches its own
     keydown handler only once the index is ready, so nothing can be searched
     before then either way.

     This keeps the field typeable during the wait and runs the query as soon
     as the index lands. The title it sets is left alone as the explanation.
     If this script fails the field simply stays disabled, which is the
     previous behaviour and is styled for (#search-input:disabled). */
  var input = document.getElementById('search-input')
  if (!input || typeof window.MutationObserver === 'undefined') return

  function enable () {
    if (input.disabled) input.disabled = false
  }

  // search-ui.js disables the field after this script runs, so watch for it
  new window.MutationObserver(enable).observe(input, {
    attributes: true,
    attributeFilter: ['disabled'],
  })
  enable()

  /* initSearch dispatches loadedindex BEFORE attaching its keydown handler
     and never replays what is already in the field, so anything typed during
     the wait would sit there until the next keystroke. Re-dispatch on the
     next tick, once that handler exists. */
  input.addEventListener('loadedindex', function () {
    if (!input.value) return
    setTimeout(function () {
      input.dispatchEvent(new window.KeyboardEvent('keydown', { bubbles: true }))
    }, 0)
  })
})()
