'use strict'

//
// Mount arbitrary directories from the repository as Antora example
// files, so `include::example$<target>/<path>[]` resolves to a file
// that lives anywhere in the source tree, not just under
// `docs/modules/<module>/examples/`.
//
// Why this exists
// ---------------
// Antora's per-module file layout normally forces docs authors to keep
// a copy of every included asset under `docs/modules/<m>/examples/` or
// `partials/`. For a project whose documentation is meant to mirror
// content from elsewhere in the same repository (golden-test snippets,
// example projects, configuration files), that means either:
//
//   * shipping duplicates and maintaining sync scripts, or
//   * relying on git symlinks, which break for users whose git is not
//     configured to materialise them.
//
// This extension reads files from configured source directories at the
// git ref of each component-version, runs each through an optional
// `.adoc` page-wrapper stripper, and registers the result in the
// content catalog as if it had always lived under
// `docs/modules/ROOT/examples/<target>/...`. The on-disk source stays
// at one canonical location.
//
// Configuration
// -------------
// In the playbook:
//
//   - require: ./extensions/repo-snippets-extension.js
//     mounts:
//       - source: tests/golden/fixtures/snippets
//         target: snippets
//         strip_page_wrapper: true
//
// `source` is the path inside the repository (relative to the repo
// root). `target` is the prefix the files appear at under `example$`.
// `strip_page_wrapper` (optional) trims the
// `= Reference / :mrdocs: / ... / [.small]#Created with MrDocs#` page
// frame that mrdocs adds, so the .adoc can be embedded inside another
// document.
//
// Per-version sourcing
// --------------------
// For each component-version, the extension determines that version's
// git ref by inspecting the `origin` of one of the existing catalog
// files for that version. Files are then read at that ref:
//
//   * worktree-backed versions (HEAD in author mode): read from the
//     local working tree, so uncommitted changes show up in preview.
//   * git-backed versions (other branches and tags): read from the ref
//     via `git show <ref>:<path>`, so v0.8.0's docs see v0.8.0's
//     snippets, not develop's.
//
// If the mount source doesn't exist at a given ref, that version is
// skipped silently — an older tag that predates the snippets directory
// simply contributes nothing.
//

const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')

const DEFAULT_COMPONENT = 'mrdocs'
const DEFAULT_MODULE = 'ROOT'

const HEADER_RE = /^= Reference\n:mrdocs:\n\n/
const FOOTER_RE = /\n*\[\.small\]#Created with https:\/\/www\.mrdocs\.com\[MrDocs\]#\s*\n*$/

const MEDIA_TYPES = {
  '.adoc': 'text/asciidoc',
  '.cpp': 'text/x-c++src',
  '.hpp': 'text/x-c++hdr',
  '.h': 'text/x-chdr',
  '.c': 'text/x-csrc',
  '.txt': 'text/plain',
  '.md': 'text/markdown',
  '.yml': 'application/yaml',
  '.yaml': 'application/yaml',
  '.json': 'application/json',
  '.xml': 'application/xml',
  '.html': 'text/html',
}

function walkLocal (dir, accumulator) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    if (entry.name.startsWith('.')) continue
    const child = path.join(dir, entry.name)
    if (entry.isDirectory()) walkLocal(child, accumulator)
    else if (entry.isFile()) accumulator.push(child)
  }
  return accumulator
}

function stripPageWrapper (text) {
  return text.replace(HEADER_RE, '').replace(FOOTER_RE, '')
}

function toPosix (p) {
  return p.split(path.sep).join('/')
}

// Discover the git ref a given component-version was aggregated from.
// Antora records this on every file's `src.origin`; pick any file from
// the version and read it off. Returns either:
//   { worktree: '<path>' }   — author-mode/local working tree
//   { gitDir: '<.git>', ref: '<refname>' } — sourced from a git ref
// or null if no information is available.
function versionOrigin (contentCatalog, component, version) {
  const sample = contentCatalog.findBy({ component, version }).find((f) => f.src && f.src.origin)
  if (!sample) return null
  const origin = sample.src.origin
  if (origin.worktree) return { worktree: origin.worktree }
  if (origin.gitdir && origin.refname) {
    return { gitDir: origin.gitdir, ref: origin.refname }
  }
  return null
}

// List every file under `sourceDir` inside the given git ref.
// `git ls-tree -r --name-only <ref> -- <path>` returns paths relative
// to the repository root.
function gitListTree (gitDir, ref, sourceDir) {
  try {
    const out = execFileSync(
      'git',
      ['--git-dir', gitDir, 'ls-tree', '-r', '--name-only', ref, '--', sourceDir],
      { encoding: 'utf-8', stdio: ['ignore', 'pipe', 'pipe'] }
    )
    return out.split('\n').filter(Boolean)
  } catch (err) {
    return null
  }
}

// Read a single file at a git ref. Returns a Buffer or null if the
// file doesn't exist at that ref.
function gitShow (gitDir, ref, relPath) {
  try {
    return execFileSync(
      'git',
      ['--git-dir', gitDir, 'show', `${ref}:${relPath}`],
      { stdio: ['ignore', 'pipe', 'pipe'] }
    )
  } catch (err) {
    return null
  }
}

module.exports.register = function ({ config }) {
  const logger = this.getLogger('repo-snippets-extension')
  const mounts = config.mounts || []
  const component = config.component || DEFAULT_COMPONENT
  const moduleName = config.module || DEFAULT_MODULE

  if (!mounts.length) {
    logger.warn('no mounts configured; extension is a no-op')
    return
  }

  this.on('contentClassified', ({ contentCatalog }) => {
    const componentObj = contentCatalog.getComponent(component)
    if (!componentObj) {
      logger.warn(`component "${component}" not found in catalog`)
      return
    }
    const componentVersions = componentObj.versions
    let added = 0
    let skipped = 0

    for (const cv of componentVersions) {
      const origin = versionOrigin(contentCatalog, cv.name, cv.version)
      if (!origin) {
        logger.debug(`no origin info for ${cv.name}@${cv.version}; skipping`)
        continue
      }

      for (const mount of mounts) {
        const sourcePath = mount.source.replace(/^\/+|\/+$/g, '')
        const targetPrefix = (mount.target || '').replace(/^\/+|\/+$/g, '')
        const stripWrapper = !!(mount.stripPageWrapper || mount.strip_page_wrapper)

        // Enumerate the (relPath, contentLoader) pairs at this version's
        // ref. For worktree origins we walk the filesystem; for git
        // origins we ask git for the tree.
        let entries
        if (origin.worktree) {
          const absSourceDir = path.join(origin.worktree, sourcePath)
          if (!fs.existsSync(absSourceDir) || !fs.statSync(absSourceDir).isDirectory()) {
            continue
          }
          entries = walkLocal(absSourceDir, []).map((abs) => ({
            relUnderSource: toPosix(path.relative(absSourceDir, abs)),
            load: () => fs.readFileSync(abs),
          }))
        } else {
          const tree = gitListTree(origin.gitDir, origin.ref, sourcePath)
          if (!tree || !tree.length) continue
          const prefix = sourcePath.endsWith('/') ? sourcePath : `${sourcePath}/`
          entries = tree
            .filter((p) => p.startsWith(prefix))
            .map((p) => ({
              relUnderSource: p.slice(prefix.length),
              load: () => gitShow(origin.gitDir, origin.ref, p),
            }))
        }

        for (const entry of entries) {
          const { relUnderSource } = entry
          if (!relUnderSource) continue
          const extname = path.extname(relUnderSource)
          const basename = path.basename(relUnderSource)
          const stem = path.basename(relUnderSource, extname)
          const relativeUnderFamily = targetPrefix
            ? `${targetPrefix}/${relUnderSource}`
            : relUnderSource

          // A per-version override on disk takes precedence; don't
          // overwrite content the version's own source tree provides.
          if (contentCatalog.getById({
            component: cv.name,
            version: cv.version,
            module: moduleName,
            family: 'example',
            relative: relativeUnderFamily,
          })) {
            skipped++
            continue
          }

          let contents = entry.load()
          if (!contents) continue
          if (stripWrapper && extname === '.adoc') {
            contents = Buffer.from(
              stripPageWrapper(contents.toString('utf-8')),
              'utf-8'
            )
          }

          contentCatalog.addFile({
            contents,
            src: {
              component: cv.name,
              version: cv.version,
              module: moduleName,
              family: 'example',
              relative: relativeUnderFamily,
              basename,
              stem,
              extname,
              mediaType: MEDIA_TYPES[extname],
              path: `modules/${moduleName}/examples/${relativeUnderFamily}`,
            },
          })
          added++
        }
      }
    }

    logger.debug(`mounted ${added} file(s); skipped ${skipped} (already in catalog)`)
  })
}
