# src/mrdocs/Metadata/

Implementation of the metadata model declared in `include/mrdocs/Metadata/`,
plus the post-extraction processing.

## Submodules
- `Symbol/` per-kind symbol implementation.
- `DocComment/` documentation-comment parsing (including `parseInlines`).
- `Finalizers/` passes that run after extraction (base-member inheritance,
  overload grouping, namespace assembly, sorting, doc-comment finalization).
