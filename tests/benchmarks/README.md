# tests/benchmarks/

Benchmarks worth keeping around so a number in the docs, or a performance claim in a PR, can be re-measured later. They are not CTest tests: timing is machine-dependent and each run takes seconds, so every benchmark is a custom target you run on demand.

## Benchmarks

- `script-engines/` — the same six transforms written in JavaScript (`addons/extensions/bench.js`) and Lua (`addons/extensions/bench.lua`), run over MrDocs's own corpus (`mrdocs.yml`, same input as `docs/mrdocs.yml`, `noop` generator). Each script times its workloads from the inside, so extraction and rendering are excluded. `run.py` runs mrdocs 30 times and prints the average per workload and engine, with the Lua-to-JavaScript ratio. The table in `docs/modules/ROOT/pages/extensions/script-engines.adoc` comes from `run.py --asciidoc`; re-run it when the engines or the bridge change and update the page.

## Running

```
cmake --build build/<preset> --target mrdocs-benchmark-script-engines
```

or directly:

```
python3 tests/benchmarks/script-engines/run.py \
    --mrdocs build/<preset>/tools/mrdocs/mrdocs \
    --addons data/mrdocs/addons --runs 30 [--asciidoc]
```

## Adding one

Give it a directory here, a `README`-worthy line above, a runner that prints per-run progress and a summary, and a custom target in `CMakeLists.txt`. Keep the workloads small enough to run in seconds and deterministic enough that an average of 30 is stable.
