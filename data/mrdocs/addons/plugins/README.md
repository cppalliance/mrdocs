# data/mrdocs/addons/plugins/

Holds the shared libraries that MrDocs loads when it is launched: every
`.dll`, `.so`, or `.dylib` directly inside it is loaded, in name order, and
asked what it provides. Any other file here, this one included, is ignored.

A supplemental addons directory can carry a `plugins` directory of its own, so
a plugin does not have to be installed next to MrDocs.

See the Plugins page of the documentation for how to write one.
