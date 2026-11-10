# tests/integration/plugin-api/

The plugin API and the plugin loader, checked from outside. The suite builds a plugin as a shared module and loads it into the `mrdocs` tool, and each case checks the exit status and the message.

The suite covers:

* a plugin that calls the exported API, and the export table of the tool.
* one plugin loaded repeatedly into two registries.
* the isolation of plugin symbols and the dependencies of a plugin.
* every plugin the loader must refuse, with the message it prints in `refused/messages/`.
* a plugin built with other standard library settings than MrDocs, which the compiler refuses.

`CMakeLists.txt` lists the cases, one numbered section each. The sources sit in `exports/`, `refused/`, `dependencies/` and `reload/`. The build registers them when tests are on, and all carry the `integration` label.

```
ctest --test-dir build/<preset> -L integration --output-on-failure
ctest --test-dir build/<preset> -R "mrdocs-plugin-.*-refused" --output-on-failure
```

To rewrite a transcript after a message changed on purpose, run its test command with `-DUPDATE=ON`.
