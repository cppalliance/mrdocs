# include/mrdocs/Metadata/

The extracted-symbol model, the core domain of MrDocs. These types describe the
C++ entities pulled out of the AST and are what generators render.

## Submodules
- `Symbol/` the symbol kinds (records, functions, variables, ...).
- `Name/` qualified-name representation.
- `Type/` the type model.
- `TParam/`, `TArg/` template parameters and arguments.
- `Specifiers/` access, storage, and other declaration specifiers.
- `DocComment/` the parsed documentation-comment model.
