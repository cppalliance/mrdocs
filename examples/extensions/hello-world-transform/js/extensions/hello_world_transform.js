mrdocs.register_transform("hello-world-transform", function(ctx) {
    ctx.report.warn("Hello from a transform");
});
