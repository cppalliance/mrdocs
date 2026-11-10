# Doc-comment fixtures

These tests check how doc comments become the doc-comment tree. Each inline or block node kind has a directory named after it, and a directory covers every input form that produces that node. The kinds are listed in `include/mrdocs/Metadata/DocComment/Inline/InlineNodes.inc` and `Block/BlockNodes.inc`. Examples:

- `image`: `![alt](src)`, the same with a title, and the HTML `<img>` tag.
- `code-span`: Markdown backticks, the Doxygen `@c` command and the HTML `<code>` tag.
- `math`: `$...$`, `\f$...\f$`, `$$...$$` and `\f[...\f]`.
- `footnote`: `[^label]` references and `[^label]: text` definitions.

They render XML only, inherited from the tree-wide `mrdocs.yml`. The matching rendering checks live in `generator/hbs/`. Where a kind's name collides with a block directory, the inline kind takes a descriptive name: inline `Code` is `code-span`, since `code` is the `@code` block.
