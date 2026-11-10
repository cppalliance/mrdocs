# data/mrdocs/addons/extensions/

MrDocs loads the pipeline extensions in this directory at launch. The order is stable for the same set of addon directories, but an extension must not rely on it. An extension is either a file or a directory, and the two follow different rules:

- A file directly in this directory is an extension when its name ends in `.lua`, `.js` or the platform's library suffix. The suffix is `.dll` on Windows, `.so` on Linux, and `.dylib` or `.so` on macOS. The name before it is free. MrDocs ignores any other file here, this one included.
- A directory is an extension named after the directory. Its entry is the one file inside called `<name>.lua`, `init.lua`, `<name>.js` or `index.js`. A library works too: `<name>` with the platform suffix, or `lib<name>` with it on Linux and macOS. The other files belong to the extension.

A supplemental addons directory can carry its own extensions directory, so you don't have to install an extension next to MrDocs.

See the Pipeline Extensions pages of the documentation for how to write one.
