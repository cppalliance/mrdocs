# Make the project compile under Mr.Docs

Mr.Docs failed to extract this project because some translation units do not
compile. Your job is to make extraction succeed.

## How extraction was invoked

```text
{command}
```

## Rules

- Fix the Mr.Docs configuration (`{config}`), the shim headers it references,
  or both. Prefer, in order: adding an include directory that provides the
  missing header, adding or extending a shim header, adding the missing
  include prefix to `missing-include-prefixes`, and only as a last resort
  excluding the file with `exclude-patterns`.
  See: https://mrdocs.com/docs/mrdocs/migration-notes.html#shim-files
- Don't change the library's own source to make it compile, except for files
  that exist only to support the documentation build.
- You can iterate quickly without a full run: compile one failing header
  directly with clang and the include flags from the command above, or copy
  the config and restrict `input:` to the failing directory.
- When you are done, reply with a one-line summary of what you changed.

## Compiler errors

{errors}
