# src/mrdocs/Support/

Private implementation of the Support utilities. It mirrors the themed
subdirectories of `include/mrdocs/Support/`: a file implementing a themed public
header lives in the matching theme (`Concurrency/`, `Error/`, `Filesystem/`,
`String/`). Cross-cutting private files that have no public counterpart (e.g.
`Report`/`ReportImpl`, `Yaml`, `Debug`, `Generator`, `ExecutionContext`) stay at
the Support root, the same way the public side keeps `Report`/`ScopeExit` at its
root.
