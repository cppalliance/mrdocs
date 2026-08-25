mrdocs.register_generator("json", function(ctx) {
  const indent = ctx.params.indent || 0;
  ctx.output.write(
    "reference.json",
    JSON.stringify(ctx.corpus.symbols, null, indent));
});
