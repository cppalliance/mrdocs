# docs/website/

The MrDocs website: a landing page with examples plus a link to the
documentation. The landing page is an `index.html` generated from
`index.html.hbs` with Handlebars, pulling its content from `data.json`. The
documentation itself is an Antora project built by the CI pipeline.

## Building the landing page

```sh
MRDOCS_ROOT=/path/to/mrdocs
export MRDOCS_ROOT
npm ci
node render.js
```

## Adding snippets

The landing page shows snippets of code with their generated documentation,
defined in the `panels` array of `data.json`. Each snippet is tested as part of
the MrDocs test suite: when the template renders, the script runs MrDocs on each
snippet and includes the output in its panel.

To add a panel, append an object to the `panels` array. Its `snippet` key is the
path of the snippet under the `snippets/` directory, and must match the name of
the symbol to document and show in the panel.
