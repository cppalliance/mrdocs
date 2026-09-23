// Script engine benchmark, JavaScript side. Six workloads over
// ctx.corpus.symbols, timed from inside the script so extraction and
// rendering are excluded. bench.lua runs the same six in the same order;
// run.py collects the "bench-js: <name> <ms> ms" lines from both.
mrdocs.register_transform("bench-js", function(ctx) {
    var s = ctx.corpus.symbols, t, r;
    t = Date.now(); var counts = {};
    for (var i = 0; i < s.length; ++i) { var k = s[i].kind; counts[k] = (counts[k] || 0) + 1; }
    console.log("bench-js: count-by-kind " + (Date.now() - t) + " ms");
    t = Date.now(); var names = [];
    for (var i = 0; i < s.length; ++i) { var sym = s[i]; if (sym.kind === "function") names.push(sym.name); }
    r = names.join(",").length;
    console.log("bench-js: collect-function-names " + (Date.now() - t) + " ms (" + names.length + ")");
    t = Date.now(); var out = "";
    for (var i = 0; i < s.length; ++i) { var sym = s[i]; out += sym.kind + " " + sym.name + "\n"; }
    console.log("bench-js: build-text " + (Date.now() - t) + " ms (" + out.length + " chars)");
    t = Date.now(); var deep = 0;
    for (var i = 0; i < s.length; ++i) { var sym = s[i]; var loc = sym.loc; if (loc && loc.def && loc.def.location && loc.def.location.lineNumber) deep += loc.def.location.lineNumber; }
    console.log("bench-js: nested-reads " + (Date.now() - t) + " ms (" + deep + ")");
    t = Date.now(); var acc = 0;
    for (var i = 0; i < 5000000; ++i) acc += i % 7;
    console.log("bench-js: numeric-loop " + (Date.now() - t) + " ms (" + acc + ")");
    t = Date.now(); var kept = [];
    for (var i = 0; i < s.length; ++i) kept.push(s[i]);
    console.log("bench-js: keep-all-proxies " + (Date.now() - t) + " ms (" + kept.length + ")");
});
