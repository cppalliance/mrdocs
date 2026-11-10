mrdocs.register_generator("hello-world-generator", function(ctx) {
    ctx.output.write("hello.txt", "Hello from a generator\n");
});
