# src/mrdocs/AST/

Clang AST traversal and symbol extraction: the front end that walks a
translation unit and produces the metadata model. `ASTVisitor` is the entry
point and switches traversal modes (regular, dependency, base-class) to control
what gets extracted; `ParseRef` parses qualified-id references; `ClangHelpers`
holds the redeclaration/definition walking helpers.
