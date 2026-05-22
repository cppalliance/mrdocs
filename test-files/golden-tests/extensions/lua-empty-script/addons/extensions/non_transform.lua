-- A file that defines globals but no `transform_corpus`. The docs
-- say MrDocs silently skips such scripts: the global below should
-- have no effect on the rendered output.

unrelated_helper = function(x) return x + 1 end
