#!/usr/bin/env bash
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Self-test of check-linux-portability.sh. The release runs the check on a
# package that is supposed to pass, which shows that the script accepts a good
# package and nothing else. This test runs the script against a stub objdump
# that prints fixed listings, for a package with the executable bin/mrdocs and
# the bundled lib/libstdc++.so.6 and lib/libgcc_s.so.1. The listings come in
# two formats: binutils 2.34 (Ubuntu 20.04, which prints the version tag of an
# undefined symbol without parentheses) and binutils 2.35 and later. The test
# expects each rejection to fire with its message, and a clean package to pass.
# If objdump changes the shape of its output so that a part of the check stops
# matching, the real package would pass silently. The listings here pin that.
#
# Two of the rejections are the messages the Invariants page shows. With
# --transcripts <dir>, the test compares the output of those cases with the
# files <dir>/runtime-version.txt and <dir>/glibc-floor.txt, which hold what
# the test expects, and with --update it rewrites them. The page shows the
# same text inline. The files live in .github/scripts/linux-portability/transcripts, and CI
# passes that directory.
#
# Usage: test-check-linux-portability.sh [--transcripts <dir> [--update]]

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
transcripts=""
update=0
while (( $# )); do
    case "$1" in
        --transcripts) transcripts="$2"; shift 2 ;;
        --update) update=1; shift ;;
        *) echo "usage: $0 [--transcripts <dir> [--update]]" >&2; exit 2 ;;
    esac
done
check="$here/check-linux-portability.sh"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/bin" "$work/pkg/bin" "$work/pkg/lib" "$work/fx"
export FIXTURE_DIR="$work/fx"

# The stub prints the listing the case put in place for the file it is asked
# about, for -p and for -T. The listings are looked up by the name of the file.
cat >"$work/bin/objdump" <<'STUB'
#!/usr/bin/env bash
name="$(basename "$2")"
case "$1" in
    -p) cat "$FIXTURE_DIR/$name.headers" ;;
    -T) cat "$FIXTURE_DIR/$name.symbols" ;;
    *) echo "stub objdump: unexpected option $1" >&2; exit 2 ;;
esac
STUB
chmod +x "$work/bin/objdump"
binary="$work/pkg/bin/mrdocs"
: >"$binary"
: >"$work/pkg/lib/libstdc++.so.6"
: >"$work/pkg/lib/libgcc_s.so.1"

# write_headers <name> <interp|static> <libc|nolibc> <runpath> [<needed>...]
# writes the output of `objdump -p` for the executable. The second argument
# says whether the binary has a program interpreter. The third says whether
# libc.so.6 is among its needed libraries. The fourth is its RUNPATH ("-" for
# none). The rest are further needed libraries.
write_headers() {
    local name="$1" interp="$2" libc="$3" runpath="$4" lib
    shift 4
    {
        echo
        echo "$name:     file format elf64-x86-64"
        echo
        echo "Program Header:"
        echo "    PHDR off    0x0000000000000040 vaddr 0x0000000000000040 paddr 0x0000000000000040 align 2**3"
        if [[ "$interp" == interp ]]; then
            echo "  INTERP off    0x0000000000000318 vaddr 0x0000000000000318 paddr 0x0000000000000318 align 2**0"
        fi
        echo "    LOAD off    0x0000000000000000 vaddr 0x0000000000000000 paddr 0x0000000000000000 align 2**12"
        echo " DYNAMIC off    0x0000000000002da0 vaddr 0x0000000000003da0 paddr 0x0000000000003da0 align 2**3"
        echo
        echo "Dynamic Section:"
        echo "  NEEDED               libm.so.6"
        echo "  NEEDED               libstdc++.so.6"
        echo "  NEEDED               libgcc_s.so.1"
        if [[ "$libc" == libc ]]; then
            echo "  NEEDED               libc.so.6"
        fi
        for lib in "$@"; do
            echo "  NEEDED               $lib"
        done
        if [[ "$runpath" != "-" ]]; then
            echo "  RUNPATH              $runpath"
        fi
        echo "  SONAME               example"
        echo
    } >"$FIXTURE_DIR/$name.headers"
}

# write_lib_headers <name> [<version>...] writes the output of `objdump -p` for
# a bundled library: a dynamic section and the version definitions, in the
# layout of binutils (a parent version is listed on an indented line).
write_lib_headers() {
    local name="$1" version n=1
    shift
    {
        echo
        echo "$name:     file format elf64-x86-64"
        echo
        echo "Dynamic Section:"
        echo "  NEEDED               libm.so.6"
        echo "  NEEDED               libc.so.6"
        echo "  SONAME               $name"
        echo
        echo "Version definitions:"
        echo "1 0x01 0x0b0e2c7e $name"
        for version in "$@"; do
            n=$((n + 1))
            echo "$n 0x00 0x08922974 $version"
            echo "	$name"
        done
        echo
        echo "Version References:"
        echo "  required from libc.so.6:"
        echo "    0x09691a75 0x00 02 GLIBC_2.2.5"
        echo
    } >"$FIXTURE_DIR/$name.headers"
}

# write_symbols <name> <old|new> [<tag> <symbol>]... writes the output of
# `objdump -T`. The second argument is the format of the binutils release (old:
# 2.34, which has no parentheses around the version tag of an undefined symbol,
# or new: 2.35 and later). Each of the rest is a version tag followed by the
# name of an undefined symbol that carries it, as one argument "TAG name".
write_symbols() {
    local name="$1" format="$2" tag symbol pair
    shift 2
    {
        echo
        echo "$name:     file format elf64-x86-64"
        echo
        echo "DYNAMIC SYMBOL TABLE:"
        echo "0000000000000000      DF *UND*	0000000000000000 (GLIBC_2.2.5) memcpy"
        echo "0000000000001139 g    DF .text	0000000000000010  Base        entry_point"
        for pair in "$@"; do
            tag="${pair%% *}"
            symbol="${pair#* }"
            if [[ "$format" == old ]]; then
                echo "0000000000000000      DF *UND*	0000000000000000  $tag $symbol"
            else
                echo "0000000000000000      DF *UND*	0000000000000000 ($tag) $symbol"
            fi
        done
    } >"$FIXTURE_DIR/$name.symbols"
}

failures=0

# record <name> <output> keeps the error lines of a rejection, with the path
# of the temporary package removed, for the transcripts.
record() {
    printf '%s\n' "$2" | grep -v '^Linux portability check failed' | sed "s#$work/pkg/##g" >"$work/$1.txt"
}

# expect <name> <status> <text> [<floor>]: runs the check on the listings in
# place, and requires the exit status and, when <text> is not empty, a line of
# the output that holds it.
expect() {
    local name="$1" status="$2" text="$3" output got=0
    output="$(PATH="$work/bin:$PATH" "$check" "$binary" "${@:4}" 2>&1)" || got=$?
    case "$name" in
        "GLIBCXX version the bundled library lacks (new)") record runtime-version "$output" ;;
        "bundled library newer than the floor (new)") record glibc-floor "$output" ;;
    esac
    if [[ "$got" != "$status" ]]; then
        echo "FAIL $name: exit status $got, expected $status"
        echo "$output" | sed 's/^/    /'
        failures=$((failures + 1))
    elif [[ -n "$text" ]] && ! grep -qF -- "$text" <<<"$output"; then
        echo "FAIL $name: the output lacks: $text"
        echo "$output" | sed 's/^/    /'
        failures=$((failures + 1))
    else
        echo "ok   $name"
    fi
}

# The package of a good build: the executable needs C++ runtime versions that
# the bundled libraries define.
good_package() {
    local format="$1"
    write_headers mrdocs interp libc '$ORIGIN/../lib'
    write_symbols mrdocs "$format" \
        "GLIBC_2.17 clock_gettime" "GLIBC_2.31 pthread_cond_clockwait" \
        "GLIBCXX_3.4.33 _ZNSt6localeC1Ev" "CXXABI_1.3.15 __cxa_throw" \
        "GCC_3.0 _Unwind_Resume"
    write_lib_headers libstdc++.so.6 GLIBCXX_3.4 GLIBCXX_3.4.33 CXXABI_1.3 CXXABI_1.3.15
    write_symbols libstdc++.so.6 "$format" "GLIBC_2.17 clock_gettime"
    write_lib_headers libgcc_s.so.1 GCC_3.0 GCC_3.3
    write_symbols libgcc_s.so.1 "$format" "GLIBC_2.17 clock_gettime"
}

for format in new old; do
    good_package "$format"
    expect "clean package ($format)" 0 "Linux portability check passed"

    write_symbols mrdocs "$format" "GLIBC_2.17 clock_gettime" "GLIBC_2.34 pthread_create"
    expect "glibc newer than the floor ($format)" 1 "needs a glibc newer than 2.31"
    expect "glibc newer than the floor names the symbol ($format)" 1 "    pthread_create"
    expect "glibc newer than the floor names the tag ($format)" 1 "GLIBC_2.34, used by:"
    expect "a higher floor accepts the same listing ($format)" 0 "Linux portability check passed" 2.34

    good_package "$format"
    write_symbols libstdc++.so.6 "$format" "GLIBC_2.38 __isoc23_strtol"
    expect "bundled library newer than the floor ($format)" 1 "libstdc++.so.6 needs a glibc newer than 2.31"

    good_package "$format"
    write_symbols mrdocs "$format" "GLIBC_2.17 clock_gettime" "GLIBC_PRIVATE _dl_find_object"
    expect "GLIBC_PRIVATE reference ($format)" 1 "refers to the glibc tag GLIBC_PRIVATE, which is not a version"
    expect "GLIBC_PRIVATE is not a version, whatever the floor ($format)" 1 "which is not a version" 2.99

    write_symbols mrdocs "$format" "GLIBC_ABI_DT_RELR __libc_start_main"
    expect "GLIBC_ABI_DT_RELR reference ($format)" 1 "refers to the glibc tag GLIBC_ABI_DT_RELR, which is not a version"

    write_symbols mrdocs "$format" "GLIBCXX_3.4.34 _ZNSt6localeC1Ev"
    expect "GLIBCXX version the bundled library lacks ($format)" 1 "that the bundled libraries do not define"
    expect "GLIBCXX version names the tag ($format)" 1 "  GLIBCXX_3.4.34"

    write_symbols mrdocs "$format" "CXXABI_1.3.16 __cxa_throw"
    expect "CXXABI version the bundled library lacks ($format)" 1 "that the bundled libraries do not define"
    expect "CXXABI version names the tag ($format)" 1 "  CXXABI_1.3.16"

    write_symbols mrdocs "$format" "GCC_4.2.0 _Unwind_Resume"
    expect "GCC version the bundled library lacks ($format)" 1 "  GCC_4.2.0"

    good_package "$format"
    write_headers mrdocs interp libc '$ORIGIN/../lib' libmvec.so.1 libanl.so.1
    expect "other libraries of glibc are accepted ($format)" 0 "Linux portability check passed"

    good_package "$format"
    write_headers mrdocs interp libc '$ORIGIN/../lib' libz.so.1
    expect "needed library outside glibc ($format)" 1 "needs libraries other than the ones of glibc"
    expect "needed library names the library ($format)" 1 "  libz.so.1"

    good_package "$format"
    write_headers mrdocs static libc '$ORIGIN/../lib'
    expect "no program interpreter ($format)" 1 "is not dynamically linked against glibc"

    write_headers mrdocs interp nolibc '$ORIGIN/../lib'
    expect "no libc.so.6 among the needed libraries ($format)" 1 "is not dynamically linked against glibc"

    write_headers mrdocs interp libc -
    expect "no RUNPATH ($format)" 1 "the RUNPATH of"
    write_headers mrdocs interp libc '/usr/lib'
    expect "RUNPATH elsewhere ($format)" 1 "'/usr/lib', not"

    good_package "$format"
    mv "$work/pkg/lib/libstdc++.so.6" "$work/libstdc++.so.6.moved"
    expect "bundled libstdc++ missing ($format)" 1 "the package lacks libstdc++.so.6"
    mv "$work/libstdc++.so.6.moved" "$work/pkg/lib/libstdc++.so.6"

    mv "$work/pkg/lib/libgcc_s.so.1" "$work/libgcc_s.so.1.moved"
    expect "bundled libgcc_s missing ($format)" 1 "the package lacks libgcc_s.so.1"
    mv "$work/libgcc_s.so.1.moved" "$work/pkg/lib/libgcc_s.so.1"
done

# sarif_summary <file>: the rule id, level, file and message of each result of
# a SARIF file, one per line, after checking the document around them.
sarif_summary() {
    python3 -I - "$1" <<'PY'
import json
import sys

doc = json.load(open(sys.argv[1]))
assert doc["version"] == "2.1.0", doc["version"]
assert doc["$schema"] == "https://json.schemastore.org/sarif-2.1.0.json"
(run,) = doc["runs"]
rules = {rule["id"]: rule["helpUri"] for rule in run["tool"]["driver"]["rules"]}
assert rules == {"linux-release": "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants/binary-compatibility.html#linux-release"}, rules
for result in run["results"]:
    uri = result["locations"][0]["physicalLocation"]["artifactLocation"]["uri"]
    print(result["ruleId"], result["level"], uri, result["message"]["text"], sep="|")
PY
}

# expect_sarif <name> <expected summary>: the SARIF file of the last
# --sarif run holds exactly the expected results.
expect_sarif() {
    local name="$1" want="$2" got
    got="$(sarif_summary "$work/out.sarif")" || got="invalid: $?"
    if [[ "$got" != "$want" ]]; then
        echo "FAIL $name: SARIF differs"
        echo "    expected: $want"
        echo "    got:      $got"
        failures=$((failures + 1))
    else
        echo "ok   $name"
    fi
}

# A failing run writes one result per failure and keeps its exit status and
# messages. A passing run writes a file with no results.
good_package new
write_symbols mrdocs new "GLIBC_2.17 clock_gettime" "GLIBC_2.34 pthread_create"
got=0
output="$(PATH="$work/bin:$PATH" "$check" --sarif "$work/out.sarif" "$binary" 2>&1)" || got=$?
if [[ "$got" != 1 ]] || ! grep -qF "needs a glibc newer than 2.31" <<<"$output"; then
    echo "FAIL SARIF run changes the exit status or the messages"
    failures=$((failures + 1))
fi
expect_sarif "SARIF of a failing run" \
    "linux-release|error|$binary|needs GLIBC_2.34, above the 2.31 floor, used by pthread_create"

good_package new
got=0
output="$(PATH="$work/bin:$PATH" "$check" --sarif "$work/out.sarif" "$binary" 2>&1)" || got=$?
if [[ "$got" != 0 ]]; then
    echo "FAIL SARIF run of a clean package: exit status $got"
    failures=$((failures + 1))
fi
expect_sarif "SARIF of a passing run" ""

good_package new
got=0
"$check" >/dev/null 2>&1 || got=$?
if [[ "$got" != 2 ]]; then
    echo "FAIL usage: exit status $got, expected 2"
    failures=$((failures + 1))
fi

if [[ -n "$transcripts" ]]; then
    for name in runtime-version glibc-floor; do
        if (( update )); then
            cp "$work/$name.txt" "$transcripts/$name.txt"
        elif ! diff -u "$transcripts/$name.txt" "$work/$name.txt"; then
            echo "FAIL transcript $name differs"
            failures=$((failures + 1))
        fi
    done
fi

if (( failures )); then
    echo "$failures check(s) failed"
    exit 1
fi
echo "All checks passed"
