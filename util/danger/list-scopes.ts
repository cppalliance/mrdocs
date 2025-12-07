//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//
/**
 * Utility script to map every tracked file to the scope Danger uses.
 * Useful for spotting files that fall into the "other" bucket.
 *
 * Usage:
 *   npm --prefix util/danger run danger:scope-map > scope-map.json
 */

import { execSync } from "node:child_process";
import path from "node:path";
import { classifyScope, scopeDisplayOrder, type ScopeKey } from "./logic";

function initBuckets(): Record<ScopeKey, string[]> {
    const buckets = {} as Record<ScopeKey, string[]>;
    for (const scope of scopeDisplayOrder) {
        buckets[scope] = [];
    }
    return buckets;
}

function main(): void {
    const repoRoot = path.resolve(__dirname, "../..");
    const output: Record<ScopeKey, string[]> = initBuckets();

    const files = execSync("git ls-files", { cwd: repoRoot })
        .toString()
        .split("\n")
        .map((line) => line.trim())
        .filter(Boolean);

    for (const file of files) {
        const scope = classifyScope(file);
        // Keep ordering stable to make diffs easy to read.
        output[scope].push(file);
    }

    for (const scope of scopeDisplayOrder) {
        output[scope].sort();
    }

    const counts = Object.fromEntries(scopeDisplayOrder.map((scope) => [scope, output[scope].length]));

    const result = {
        counts,
        files: output,
    };

    // Pretty-print so it can be inspected or diffed easily.
    console.log(JSON.stringify(result, null, 2));
}

main();
