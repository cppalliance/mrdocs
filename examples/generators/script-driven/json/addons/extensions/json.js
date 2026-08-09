// A script-driven generator that emits the whole corpus as one JSON file.
//
// Every symbol reaches the script as plain data on `ctx.corpus.symbols`, so
// emitting it is a single call to the standard `JSON.stringify`. That is the
// whole generator: no C++, no templates.
//
// Because the script owns the emit, the shape is yours to change. Map or
// filter the array before stringifying, or read settings from `ctx.params`
// (the `generator-options.json` block). Here an optional `indent` pretty-prints
// the output.
//
// The id is `json`, the same as the built-in JSON generator. A script
// generator takes precedence over a built-in of the same name, so this
// replaces it; pick a different id to add a format alongside the built-ins.

mrdocs.register_generator("json", function(ctx) {
  const indent = ctx.params.indent || 0;
  ctx.output.write(
    "reference.json",
    JSON.stringify(ctx.corpus.symbols, null, indent));
});
