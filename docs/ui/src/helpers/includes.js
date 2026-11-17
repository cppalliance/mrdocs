'use strict'

module.exports = (haystack, needle) => typeof haystack === 'string' && haystack.includes(needle)
