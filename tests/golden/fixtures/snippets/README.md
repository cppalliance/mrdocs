# Documentation snippets

Each `.cpp` here is a short source whose output the documentation embeds, so it reads well on the page. Edge cases and exhaustive coverage belong in `generator/hbs/` or the XML tests.

The tree-wide `mrdocs.yml` defaults this directory to `adoc`. A directory or snippet can override it. Each area renders only the format its page embeds:

- `commands/`: adoc only, for the command reference pages under `docs/modules/ROOT/pages/commands/`. Put html or xml coverage in a `generator/hbs/` or XML test.
- `landing/`: adoc and html, for the Antora home page, `docs/modules/ROOT/pages/index.adoc`.
- `*.cpp` in this directory: html, for the panels `docs/website/render.js` embeds.
