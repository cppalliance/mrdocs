# data/lldb/

LLDB data formatters (summaries and synthetic children) for MrDocs types. Load
them from your `.lldbinit`:

```
command script import /path/share/lldb/mrdocs_formatters.py
type category enable MrDocs
```

Importing the module runs `__lldb_init_module`, which registers the formatters
and enables the `MrDocs` type category, so the second line is harmless
redundancy.

To hot-reload the formatters while editing them during a session:

```
script import importlib, mrdocs_formatters; importlib.reload(mrdocs_formatters)
```
