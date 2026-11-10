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

# Check that the Linux package follows the portability policy of the release.
# The argument is the executable of the package, bin/mrdocs. The script looks
# for the C++ runtime the package ships in the lib/ directory next to bin/.
# Six parts:
#
#   1. The executable is dynamically linked against glibc. A static glibc
#      loads a plugin only on a system whose shared glibc is the exact version
#      it was linked with, so a static or static-pie binary is rejected.
#   2. Neither the executable nor a bundled library refers to a glibc symbol
#      newer than the floor.
#   3. Neither refers to a GLIBC_ tag that is not a version (GLIBC_PRIVATE,
#      the internals of glibc, which change between builds, or
#      GLIBC_ABI_DT_RELR, left by packed relative relocations, which older
#      loaders do not support), since the floor does not cover such a tag.
#   4. lib/libstdc++.so.6 and lib/libgcc_s.so.1 exist, and the RUNPATH of the
#      executable is $ORIGIN/../lib, which is where it finds them.
#   5. The bundled libraries define every GLIBCXX_, CXXABI_ and GCC_ version
#      the executable refers to, so the executable never needs a C++ runtime
#      from the system.
#   6. The executable and the bundled libraries need no library except the
#      libraries of glibc and the two bundled ones.
#
# This is the check auditwheel makes on a wheel for the manylinux policy, in a
# stricter form. manylinux whitelists the system libstdc++ up to capped symbol
# versions, and this release ships its own copy instead. The policy, its origin
# and the reason for the floor are in utils/cmake/release-linux.cmake and
# docs/modules/ROOT/pages/contribute/invariants/binary-compatibility.adoc.
#
# If the check fails, fix the package. Do not raise the floor to make it pass.
#
# Usage: check-linux-portability.sh [--sarif <file>] <executable> [<glibc-floor>]
#
# The floor defaults to 2.31, the glibc of Ubuntu 20.04.
#
# With --sarif the script also writes each failure to <file> as SARIF, under
# the rule linux-release of the invariants page. The script keeps one tab
# separated line per failure and hands the lines to utils/lint/sarif.py,
# so the JSON is written in one place for every guard. The messages and the
# exit status do not change.

set -euo pipefail

FLOOR_DEFAULT="2.31"
RUNPATH_EXPECTED='$ORIGIN/../lib'

sarif_out=""
if [[ "${1:-}" == "--sarif" ]]; then
    if [[ $# -lt 2 ]]; then
        echo "usage: $0 [--sarif <file>] <executable> [<glibc-floor>]" >&2
        exit 2
    fi
    sarif_out="$2"
    shift 2
fi

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 [--sarif <file>] <executable> [<glibc-floor>]" >&2
    exit 2
fi

binary="$1"
floor="${2:-$FLOOR_DEFAULT}"

if [[ ! -f "$binary" ]]; then
    echo "error: $binary is not a file" >&2
    exit 2
fi
if ! command -v objdump >/dev/null 2>&1; then
    echo "error: objdump is not installed (package binutils)" >&2
    exit 2
fi
if [[ ! "$floor" =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
    echo "error: the glibc floor '$floor' is not a version number" >&2
    exit 2
fi

libdir="$(dirname "$binary")/../lib"
bundled=(libstdc++.so.6 libgcc_s.so.1)

# The failures, one line each: rule, level, file, line (empty), message.
findings=""
sarif_py="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../../utils/lint/sarif.py"

# record <file> <message>: remember a failure for the SARIF file.
record() {
    findings+="$(printf 'linux-release\terror\t%s\t\t%s' "$1" "$2")"$'\n'
}

# write_sarif: write the failures recorded so far, when --sarif asked for it.
write_sarif() {
    if [[ -n "$sarif_out" ]]; then
        if ! python3 -I "$sarif_py" --tool check-linux-portability \
            --rule linux-release --output "$sarif_out" <<<"$findings"; then
            echo "warning: could not write the SARIF file $sarif_out" >&2
        fi
    fi
}

# The program headers and the dynamic section of the binary.
headers="$(objdump -p "$binary")"

# The binary has to be dynamically linked against glibc. A static glibc loads a
# plugin only on a system whose shared glibc is the exact version it was
# linked with, and a static binary has no version tags or needed libraries
# for the other parts of the check to find. A static binary has no program
# interpreter, and a static-pie one has no libc.so.6 in its needed libraries.
if ! grep -q '^ *INTERP ' <<<"$headers" ||
   ! awk '$1 == "NEEDED" && $2 == "libc.so.6" {found = 1} END {exit !found}' <<<"$headers"; then
    echo "error: $binary is not dynamically linked against glibc (a static binary)." >&2
    echo "  A static glibc loads plugins only on a system whose shared glibc is the" >&2
    echo "  exact version it was linked with, so the release links glibc dynamically" >&2
    echo "  and ships the C++ runtime in lib/: see utils/cmake/release-linux.cmake" >&2
    echo "Linux portability check failed for $binary (glibc floor $floor)" >&2
    record "$binary" "not dynamically linked against glibc: a static glibc loads plugins only where the shared glibc is the same version"
    write_sarif
    exit 1
fi

# The libraries glibc is made of, which the floor already covers, and the two
# the package bundles. Anything else in the list of needed libraries is a
# dependency on the system.
allowed_needed='^(libc\.so\.6|libm\.so\.6|libpthread\.so\.0|libdl\.so\.2|librt\.so\.1|libutil\.so\.1|libresolv\.so\.2|libmvec\.so\.1|libanl\.so\.1|libstdc\+\+\.so\.6|libgcc_s\.so\.1|ld-linux[A-Za-z0-9_.-]*\.so\.[0-9]+|ld64\.so\.[0-9]+)$'

failed=0

# True when version $1 is greater than version $2.
version_greater() {
    [[ "$1" != "$2" && "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n 1)" == "$1" ]]
}

# check_glibc <file> <symbols>: every GLIBC_ tag in the dynamic symbols of the
# file. A tag that is a version, GLIBC_x.y[.z], is compared with the floor. A
# tag that is not, such as GLIBC_PRIVATE or GLIBC_ABI_DT_RELR, is no version of
# the interface the floor covers, so it fails the check on its own.
check_glibc() {
    local file="$1" symbols="$2" tag version users
    local newer=()
    while read -r tag; do
        [[ -z "$tag" ]] && continue
        version="${tag#GLIBC_}"
        if [[ ! "$version" =~ ^[0-9]+(\.[0-9]+)+$ ]]; then
            echo "error: $file refers to the glibc tag $tag, which is not a version" >&2
            record "$file" "refers to the glibc tag $tag, which is not a version"
            failed=1
            continue
        fi
        if version_greater "$version" "$floor"; then
            newer+=("$tag")
        fi
    done < <(grep -o 'GLIBC_[A-Za-z0-9_.]*' <<<"$symbols" | sort -u || true)

    if (( ${#newer[@]} > 0 )); then
        failed=1
        echo "error: $file needs a glibc newer than $floor:" >&2
        for tag in "${newer[@]}"; do
            echo "  $tag, used by:" >&2
            # objdump -T prints "address flags section address version-tag name".
            # Newer binutils put the tag in parentheses for an undefined symbol,
            # and binutils 2.34, the one of Ubuntu 20.04, does not. The check
            # therefore matches the tag as a field with or without them.
            users="$(awk -v t="$tag" '{
                for (i = 1; i < NF; ++i) {
                    f = $i
                    gsub(/[()]/, "", f)
                    if (f == t) { print $NF; break }
                }
            }' <<<"$symbols" | sort -u)"
            if [[ -n "$users" ]]; then
                sed 's/^/    /' <<<"$users" >&2
            fi
            record "$file" "needs $tag, above the $floor floor, used by $(tr '\n' ' ' <<<"$users" | sed 's/ *$//; s/ /, /g')"
        done
    fi
}

# check_needed <file> <headers>: the needed libraries of a file are libraries
# of glibc or one of the bundled two.
check_needed() {
    local file="$1" headers="$2" lib
    local unexpected=()
    while read -r lib; do
        [[ -z "$lib" ]] && continue
        if [[ ! "$lib" =~ $allowed_needed ]]; then
            unexpected+=("$lib")
        fi
    done < <(awk '$1 == "NEEDED" {print $2}' <<<"$headers" | sort -u)
    if (( ${#unexpected[@]} > 0 )); then
        failed=1
        echo "error: $file needs libraries other than the ones of glibc and the bundled C++ runtime:" >&2
        for lib in "${unexpected[@]}"; do
            echo "  $lib" >&2
        done
        record "$file" "needs libraries other than the ones of glibc and the bundled C++ runtime: $(IFS=,; echo "${unexpected[*]}" | sed 's/,/, /g')"
    fi
}

# The dynamic symbols the binary refers to or defines, with their versions.
symbols="$(objdump -T "$binary")"
check_glibc "$binary" "$symbols"
check_needed "$binary" "$headers"

# The executable finds the bundled libraries through its RUNPATH.
runpath="$(awk '$1 == "RUNPATH" {print $2}' <<<"$headers")"
if [[ "$runpath" != "$RUNPATH_EXPECTED" ]]; then
    failed=1
    echo "error: the RUNPATH of $binary is '${runpath:-(none)}', not '$RUNPATH_EXPECTED'," >&2
    echo "  so it does not find the C++ runtime in lib/" >&2
    record "$binary" "the RUNPATH is '${runpath:-(none)}', not '$RUNPATH_EXPECTED', so it does not find the C++ runtime in lib/"
fi

# The bundled libraries, with the versions they define, and the floor and the
# needed libraries of each.
defined=""
for lib in "${bundled[@]}"; do
    path="$libdir/$lib"
    if [[ ! -f "$path" ]]; then
        failed=1
        echo "error: the package lacks $lib: $path is not a file" >&2
        record "$path" "the package lacks $lib"
        continue
    fi
    lib_headers="$(objdump -p "$path")"
    check_glibc "$path" "$(objdump -T "$path")"
    check_needed "$path" "$lib_headers"
    # The names of the versions a library defines are the lines between
    # "Version definitions:" and the next section of `objdump -p`.
    defined+="$(awk '
        /^Version definitions:/ {in_defs = 1; next}
        /^[A-Za-z]/ {in_defs = 0}
        in_defs {for (i = 1; i <= NF; ++i) print $i}
    ' <<<"$lib_headers")"$'\n'
done

# The bundled libraries define every version of the C++ runtime the
# executable refers to, so the executable never needs one from the system.
undefined=()
while read -r ref; do
    [[ -z "$ref" ]] && continue
    if ! grep -qxF -- "$ref" <<<"$defined"; then
        undefined+=("$ref")
    fi
done < <(grep -o -E '(GLIBCXX|CXXABI|GCC)_[0-9][0-9.]*' <<<"$symbols" | sort -u || true)
if (( ${#undefined[@]} > 0 )); then
    failed=1
    echo "error: $binary refers to C++ runtime versions that the bundled libraries do not define:" >&2
    for ref in "${undefined[@]}"; do
        echo "  $ref" >&2
    done
    record "$binary" "refers to C++ runtime versions that the bundled libraries do not define: $(IFS=,; echo "${undefined[*]}" | sed 's/,/, /g')"
fi

write_sarif

if (( failed )); then
    echo "Linux portability check failed for $binary (glibc floor $floor)" >&2
    exit 1
fi

echo "Linux portability check passed for $binary (glibc floor $floor)"
