#!/usr/bin/env python3
"""Run the script engine benchmarks and print a table of averages.

Runs mrdocs over MrDocs's own corpus with bench.js and bench.lua loaded,
30 times by default, and reports the mean of each workload per engine plus
the Lua-to-JavaScript ratio. The scripts time themselves, so only the
scripts' own work is measured.

    run.py --mrdocs build/release-macos/tools/mrdocs/mrdocs \\
           --addons data/mrdocs/addons [--runs 30] [--asciidoc]
"""
import argparse
import os
import re
import statistics
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
LINE = re.compile(r"^bench-(js|lua): (\S+) (\d+) ms")


def run_once(mrdocs, addons, output):
    cmd = [
        mrdocs,
        f"--config={os.path.join(HERE, 'mrdocs.yml')}",
        f"--output={output}",
    ]
    if addons:
        cmd.append(f"--addons={addons}")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(f"mrdocs exited with {proc.returncode}")
    results = {}
    for line in (proc.stdout + proc.stderr).splitlines():
        m = LINE.search(line.replace("\x1b[0m", ""))
        if m:
            results[(m.group(1), m.group(2))] = int(m.group(3))
    if not results:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit("no bench lines found in the mrdocs output")
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--mrdocs", required=True, help="path to the mrdocs binary")
    ap.add_argument("--addons", help="path to the default addons directory")
    ap.add_argument("--runs", type=int, default=30)
    ap.add_argument("--asciidoc", action="store_true",
                    help="print an AsciiDoc table instead of Markdown")
    args = ap.parse_args()
    # mrdocs resolves a relative --addons against the config file, so
    # make both paths absolute here.
    args.mrdocs = os.path.abspath(args.mrdocs)
    if args.addons:
        args.addons = os.path.abspath(args.addons)

    samples = {}
    with tempfile.TemporaryDirectory() as tmp:
        for i in range(args.runs):
            print(f"run {i + 1}/{args.runs} ...", file=sys.stderr, flush=True)
            for key, ms in run_once(args.mrdocs, args.addons, tmp).items():
                samples.setdefault(key, []).append(ms)

    names = []
    for engine, name in samples:
        if name not in names:
            names.append(name)
    rows = []
    for name in names:
        js = statistics.fmean(samples.get(("js", name), [float("nan")]))
        lua = statistics.fmean(samples.get(("lua", name), [float("nan")]))
        ratio = js / lua if lua else float("inf")
        rows.append((name, js, lua, ratio))

    if args.asciidoc:
        print('[cols="3,1,1,1"]\n|===\n| Workload | JavaScript | Lua | Ratio\n')
        for name, js, lua, ratio in rows:
            print(f"| {name}\n| {js:.0f} ms\n| {lua:.0f} ms\n| {ratio:.0f}×\n")
        print("|===")
    else:
        print(f"| Workload | JavaScript | Lua | Ratio |")
        print(f"|---|---|---|---|")
        for name, js, lua, ratio in rows:
            print(f"| {name} | {js:.0f} ms | {lua:.0f} ms | {ratio:.0f}× |")
    print(f"\naverage of {args.runs} runs, in-script timing", file=sys.stderr)


if __name__ == "__main__":
    main()
