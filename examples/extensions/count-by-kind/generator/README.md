# examples/extensions/count-by-kind/generator/

A plugin that counts symbols by kind, the project it runs on, and the output it writes for that project.

The generator `stats` writes one line per symbol kind with its count. A last line gives the number of symbols that have a brief. `sample-project/` also holds `stats.txt`, the output of that run.

Build the library into `<dir>/extensions/stats/`, where `<dir>` is a directory of your own, then run:

```
mrdocs --config=sample-project/mrdocs.yml --addons-supplemental=<dir>
```

The `mrdocs-plugin-example-stats` test builds the library and documents `sample-project/` with it. `mrdocs-plugin-example-stats-output` checks that the generator wrote `stats.txt`.

The Defining Extensions page of the documentation shows its entry point and how to build it against an installed MrDocs.
