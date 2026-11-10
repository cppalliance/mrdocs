'use strict'

//
// Resolve `include::example$<target>/<path>[]` to a file of the repository
// outside `docs/modules/<module>/examples/`.
//
// A mount in the playbook is a prefix:
//
//   - require: ./extensions/repo-snippets-extension.js
//     mounts:
//       - source: tests/golden/fixtures/snippets
//         target: snippets
//         strip_page_wrapper: true
//
// `source` is a directory of the repository (relative to the repo root) and
// `target` is the prefix an include names under `example$`, so
// `include::example$snippets/x.cpp[]` reads
// `tests/golden/fixtures/snippets/x.cpp`.
// `strip_page_wrapper` (optional) trims the page frame that mrdocs adds
// (`= Reference`, `:mrdocs:`, `[.small]#Created with MrDocs#`), so the .adoc
// can be embedded inside another document.
//
// Antora resolves a resource-ID include through `ContentCatalog.getById`:
// `resolveResource` calls it for every include, and for its alias fallback.
// The extension wraps `getById` on the catalog instance. When the catalog has
// no example resource for an id under the target of a mount, the wrapper
// reads that one file, adds it to the catalog and returns it. Each origin
// reads its own way:
//
//   * a worktree (HEAD in author mode) reads the file from disk, unless git
//     ignores it, so uncommitted changes show up in preview and a file that a
//     published build would not have is refused;
//   * a git ref (other branches and tags) reads it with
//     `git show <ref>:<path>`, so v0.8.0's docs see v0.8.0's snippets.
//
// Limits: only `example` resources of the ROOT module resolve, a path outside
// every mount or a file that does not exist resolves to nothing (Antora then
// reports its usual include error), and a relative include inside a mounted
// file does not resolve through the mount.
//

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const DEFAULT_COMPONENT = 'mrdocs'
const DEFAULT_MODULE = 'ROOT'
const MAX_BUFFER = 64 * 1024 * 1024
const MISSING_PATH_RE = /exists on disk, but not in|does not exist in/

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

function stripPageWrapper (text) {
  return text.replace(HEADER_RE, '').replace(FOOTER_RE, '')
}

// Discover the git ref a component-version was aggregated from. Antora
// records it on every file's `src.origin`. Returns either
//   { worktree: '<path>' }                 for a local working tree,
//   { gitDir: '<.git>', ref: '<ref>' }     for a git ref,
// or null if no information is available.
function versionOrigin (contentCatalog, component, version) {
  const sample = contentCatalog.findBy({ component, version }).find((f) => f.src && f.src.origin)
  if (!sample) return null
  const origin = sample.src.origin
  if (origin.worktree) return { worktree: origin.worktree }
  if (origin.gitdir && (origin.refhash || origin.refname)) {
    return { gitDir: origin.gitdir, ref: origin.refhash || origin.refname }
  }
  return null
}

// Read a file at a git ref. Returns a Buffer, or null if the path does not
// exist at that ref. Any other git failure throws with git's message.
function gitShow (gitDir, ref, relPath) {
  const result = spawnSync(
    'git',
    ['--git-dir', gitDir, 'show', `${ref}:${relPath}`],
    { stdio: ['ignore', 'pipe', 'pipe'], maxBuffer: MAX_BUFFER }
  )
  if (result.error) throw result.error
  if (result.status === 0) return result.stdout
  const stderr = result.stderr.toString('utf-8')
  if (MISSING_PATH_RE.test(stderr)) return null
  throw new Error(`repo-snippets: git show ${ref}:${relPath} failed: ${stderr.trim()}`)
}

// True when git ignores the path in a working tree. A tree that is not a
// git checkout ignores nothing.
function gitIgnores (worktree, relPath) {
  const result = spawnSync('git', ['-C', worktree, 'check-ignore', '-q', '--', relPath], { stdio: 'ignore' })
  return result.status === 0
}

// Read `relPath` (relative to the repository root) from an origin.
function readFile (origin, relPath) {
  if (!origin.worktree) return gitShow(origin.gitDir, origin.ref, relPath)
  if (gitIgnores(origin.worktree, relPath)) return null
  try {
    return fs.readFileSync(path.join(origin.worktree, relPath))
  } catch (err) {
    return null
  }
}

// Split an include path into the mount it names and the path under the
// mount's source. Returns null when no mount matches.
function matchMount (mounts, relative) {
  for (const mount of mounts) {
    const prefix = `${mount.target}/`
    if (!relative.startsWith(prefix)) continue
    const rest = path.posix.normalize(relative.slice(prefix.length))
    if (!rest || rest === '.' || rest === '..' || rest.startsWith('../') || path.posix.isAbsolute(rest)) return null
    return { mount, rest }
  }
  return null
}

module.exports.register = function ({ config }) {
  const logger = this.getLogger('repo-snippets-extension')
  const component = config.component || DEFAULT_COMPONENT
  const moduleName = config.module || DEFAULT_MODULE
  const mounts = (config.mounts || []).map((mount) => ({
    source: mount.source.replace(/^\/+|\/+$/g, ''),
    target: (mount.target || '').replace(/^\/+|\/+$/g, ''),
    stripWrapper: !!(mount.stripPageWrapper || mount.strip_page_wrapper),
  }))

  if (!mounts.length) {
    logger.warn('no mounts configured; extension is a no-op')
    return
  }

  this.on('contentClassified', ({ contentCatalog }) => {
    const componentObj = contentCatalog.getComponent(component)
    if (!componentObj) throw new Error(`repo-snippets: component "${component}" not found in the catalog`)
    for (const mount of mounts) {
      if (!mount.target) throw new Error(`repo-snippets: mount "${mount.source}" has no target`)
    }

    const origins = new Map()
    const misses = new Map()
    for (const { version } of componentObj.versions) {
      const origin = versionOrigin(contentCatalog, component, version)
      origins.set(version, origin)
      misses.set(version, new Set())
      if (!origin || !origin.worktree) continue
      for (const mount of mounts) {
        const absSource = path.join(origin.worktree, mount.source)
        if (fs.existsSync(absSource) && !fs.statSync(absSource).isDirectory()) {
          throw new Error(`repo-snippets: mount source "${mount.source}" is not a directory`)
        }
      }
    }

    const original = contentCatalog.getById.bind(contentCatalog)
    contentCatalog.getById = function getById (id) {
      const found = original(id)
      if (found || !id || id.family !== 'example' || id.module !== moduleName ||
          id.component !== component || typeof id.relative !== 'string') return found

      const origin = origins.get(id.version)
      const missed = misses.get(id.version)
      if (!origin || missed.has(id.relative)) return found
      const match = matchMount(mounts, id.relative)
      if (!match) return found

      const relative = `${match.mount.target}/${match.rest}`
      const known = relative === id.relative ? undefined : original(Object.assign({}, id, { relative }))
      if (known) return known

      let contents = readFile(origin, `${match.mount.source}/${match.rest}`)
      if (!contents) {
        missed.add(id.relative)
        return found
      }

      const extname = path.posix.extname(match.rest)
      if (match.mount.stripWrapper && extname === '.adoc') {
        contents = Buffer.from(stripPageWrapper(contents.toString('utf-8')), 'utf-8')
      }
      const src = {
        component: id.component,
        version: id.version,
        module: moduleName,
        family: 'example',
        relative,
        basename: path.posix.basename(relative),
        stem: path.posix.basename(relative, extname),
        extname,
        path: `modules/${moduleName}/examples/${relative}`,
      }
      if (MEDIA_TYPES[extname]) src.mediaType = MEDIA_TYPES[extname]
      return contentCatalog.addFile({ contents, src })
    }
  })
}
