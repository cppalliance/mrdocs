-- A file that defines globals but registers no transform. MrDocs warns
-- that it had no effect but still completes the build: the global below
-- does not change the rendered output.

unrelated_helper = function(x) return x + 1 end
