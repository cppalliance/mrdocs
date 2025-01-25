'use strict'

module.exports = (numOfItems, { data }) => {
  const { contentCatalog, site } = data.root
  if (!contentCatalog) return
  const rawPages = getDatedReleaseNotesRawPages(contentCatalog)
  const pageUiModels = turnRawPagesIntoPageUiModels(site, rawPages, contentCatalog)
  return getMostRecentlyUpdatedPages(pageUiModels, numOfItems)
}

let buildPageUiModel

function getDatedReleaseNotesRawPages (contentCatalog) {
  return contentCatalog.getPages(({ asciidoc, out }) => {
    if (!asciidoc || !out) return
    return getReleaseNotesWithRevdate(asciidoc)
  })
}

function getReleaseNotesWithRevdate (asciidoc) {
  const attributes = asciidoc.attributes
  return asciidoc.attributes && isReleaseNotes(attributes) && hasRevDate(attributes)
}

function isReleaseNotes (attributes) {
  return attributes['page-component-name'] === 'release-notes'
}

function hasRevDate (attributes) {
  return 'page-revdate' in attributes
}

function turnRawPagesIntoPageUiModels (site, pages, contentCatalog) {
  buildPageUiModel ??= module.parent.require('@antora/page-composer/build-ui-model').buildPageUiModel
  return pages
    .map((page) => buildPageUiModel(site, page, contentCatalog))
    .filter((page) => isValidDate(page.attributes?.revdate))
    .sort(sortByRevDate)
}

function isValidDate (dateStr) {
  return !isNaN(Date.parse(dateStr))
}

function sortByRevDate (a, b) {
  return new Date(b.attributes.revdate) - new Date(a.attributes.revdate)
}

function getMostRecentlyUpdatedPages (pageUiModels, numOfItems) {
  return getResultList(pageUiModels, Math.min(pageUiModels.length, numOfItems))
}

function getResultList (pageUiModels, maxNumberOfPages) {
  const resultList = []
  for (let i = 0; i < maxNumberOfPages; i++) {
    const page = pageUiModels[i]
    if (page.attributes?.revdate) resultList.push(getSelectedAttributes(page))
  }
  return resultList
}

function getSelectedAttributes (page) {
  const latestVersion = getLatestVersion(page.contents.toString())
  return {
    latestVersionAnchor: latestVersion?.anchor,
    latestVersionName: latestVersion?.innerText,
    revdateWithoutYear: removeYear(page.attributes?.revdate),
    title: cleanTitle(page.title),
    url: page.url,
  }
}

function getLatestVersion (contentsStr) {
  const firstVersion = contentsStr.match(/<h2 id="([^"]+)">(.+?)<\/h2>/)
  if (!firstVersion) return
  const result = { anchor: firstVersion[1] }
  if (isVersion(firstVersion[2])) result.innerText = firstVersion[2]
  return result
}

function isVersion (versionText) {
  return /^[0-9]+\.[0-9]+(?:\.[0-9]+)?/.test(versionText)
}

function removeYear (dateStr) {
  if (!isValidDate(dateStr)) return
  const dateObj = new Date(dateStr)
  return `${dateObj.toLocaleString('default', { month: 'short' })} ${dateObj.getDate()}`
}

function cleanTitle (title) {
  return title.split('Release Notes')[0].trim()
}
