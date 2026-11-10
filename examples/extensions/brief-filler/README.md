# examples/extensions/brief-filler/

A plugin that gives each undocumented symbol a placeholder brief, and the project it runs on.

The transform, `brief-filler`, fills the brief of every symbol that has none. The text comes from `transform-options.brief-filler.text` in the configuration, or is "Undocumented." without one. `sample-project/` holds a function nobody documented.

Build the library into `<dir>/extensions/brief-filler/`, where `<dir>` is a directory of your own, then run:

```
mrdocs --config=sample-project/mrdocs.yml --addons-supplemental=<dir>
```

The Corpus Transforms page of the documentation shows the `transform-options` block a transform reads.
