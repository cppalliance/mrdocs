# examples/extensions/deprecations/

A plugin that reports deprecated symbols and fills missing briefs from the deprecation message, and the project it runs on.

- The generator `deprecations` writes one entry per symbol marked `[[deprecated]]`.
- The transform `deprecation-brief` uses the message as the brief of a deprecated symbol that has none.
- `sample-project/` holds the C++ they document and `deprecations.txt`, the output the generator writes for it.

Build the library into `<dir>/extensions/deprecations/`, where `<dir>` is a directory of your own, then run:

```
mrdocs --config=sample-project/mrdocs.yml --addons-supplemental=<dir>
```

The Programmatic Generators page of the documentation walks through the code.
