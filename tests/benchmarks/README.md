# tests/benchmarks/

Benchmarks kept so a number in the docs, or a performance claim in a PR, can be re-measured later. They are not CTest tests, since timing depends on the machine, so each is a custom target you run on demand.

## Benchmarks

`script-engines/` holds the same transforms written in JavaScript and Lua, run over the corpus of MrDocs itself with the `noop` generator. Each script times its workloads from the inside, so extraction and rendering are excluded.

`run.py` runs mrdocs the number of times its option defaults to and prints the average per workload and engine, with the Lua-to-JavaScript ratio. The table in the Defining Extensions page comes from `run.py --asciidoc`. Re-run it when the engines or the bridge change.

## Running

```
cmake --build build/<preset> --target mrdocs-benchmark-script-engines
```

`run.py --help` lists the options when you run it directly.

## Adding one

Give it a directory here, a line above, a runner that prints per-run progress and a summary, and a custom target in `CMakeLists.txt`. Keep the workloads small and deterministic enough that an average over the default run count is stable.
