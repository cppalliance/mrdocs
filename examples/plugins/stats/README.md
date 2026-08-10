# examples/plugins/stats/

A plugin, and a project documented with it. The two are separate things, and
this directory holds one of each:

- plugin.cpp is the plugin. It installs a generator, `stats`, which writes one
  line per symbol kind saying how many symbols of that kind the corpus has.
- sample-project/ is the project it is run on: a little C++ to document, an
  mrdocs.yml that asks for `generator: stats`, and stats.txt, what the
  generator wrote for that input.

Building the library and documenting the sample project with it is what the
`mrdocs-plugin-example-stats` test does. By hand, it is the same two steps:
build the library into the plugins subdirectory of a directory of your own,
then name that directory when you run MrDocs.

```
mrdocs --config=sample-project/mrdocs.yml --addons-supplemental=<that directory>
```

See the Plugins page of the documentation for what the code is doing, and for
how to build the library against an installed MrDocs rather than this one.
