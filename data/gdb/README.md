# data/gdb/

GDB pretty-printers for MrDocs types. Load them from your `.gdbinit`:

```python
python
import sys
sys.path.insert(0, '/path/share/gdb')                 # directory with the printers
from mrdocs_printers import register_mrdocs_printers   # the registration function
register_mrdocs_printers()                             # register them
end
```

The printers register only when you call `register_mrdocs_printers` yourself,
which keeps registration separate from importing the module.

Note: the printers require Python 3.
