# include/mrdocs/ADT/

Abstract data types: small, dependency-light value types reused across the
public API. Examples: `Optional` (a nullable value), `Polymorphic` (a value-like
owning polymorphic holder), `BitField`. These are header-mostly and avoid
pulling LLVM into the public surface.
