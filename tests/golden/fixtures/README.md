# Golden tests

Fixtures for the golden tests: each directory holds `.cpp` inputs, an `mrdocs.yml`, and the expected output the runner (`mrdocs-golden-tests`) compares byte for byte. The [testing page](../../../docs/modules/ROOT/pages/contribute/testing.adoc) explains how to add, validate and update them. Never hand-edit the generated output.

Most tests render XML only, because it records everything extracted and catches regressions cheaply. Add adoc or html goldens only where the rendered output is the point.

- `config/`: one directory per option.
- `core/`: empty input, libc++, UTF-8, canonical ordering.
- `extensions/`: JavaScript and Lua extensions.
- `external-include/`: a header outside the source root.
- `filters/`: file, symbol-name and symbol-type filters.
- `generator/hbs/`: Handlebars rendering tests, see its README.
- `generators/`: generator-specific output.
- `javadoc/`: doc-comment node kinds, see its README.
- `snippets/`: sources embedded in the docs, see its README.
- `symbols/`: one directory per symbol kind.
- `templates/`: template declarations and specializations.
