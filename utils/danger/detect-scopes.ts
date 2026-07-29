//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//
/**
 * Classify changed files into Danger scopes and decide whether the change
 * can affect the produced mrdocs binary. Reads newline-separated paths from
 * stdin and prints a JSON object to stdout that CI consumes via `jq`:
 *
 *   { "scopes": [...], "is_code_change": bool }
 *
 * Usage:
 *   git diff --name-only origin/develop...HEAD | \
 *     npx --prefix utils/danger ts-node utils/danger/detect-scopes.ts
 */

import { affectsBuildPipeline, classifyScope, isCodeChange, scopeDisplayOrder, scopesTouched } from "./logic";

async function readStdin(): Promise<string> {
    const chunks: Buffer[] = [];
    for await (const chunk of process.stdin) {
        chunks.push(typeof chunk === "string" ? Buffer.from(chunk) : chunk);
    }
    return Buffer.concat(chunks).toString("utf8");
}

function main(): void {
    void readStdin().then((input) => {
        const paths = input
            .split(/\r?\n/)
            .map((line) => line.trim())
            .filter(Boolean);

        const scopes = scopesTouched(paths);
        const orderedScopes = scopeDisplayOrder.filter((scope) => scopes.has(scope));

        const result = {
            scopes: orderedScopes,
            is_code_change: isCodeChange(scopes) || affectsBuildPipeline(paths),
            file_count: paths.length,
        };

        process.stdout.write(JSON.stringify(result) + "\n");
    });
}

// Keep classifyScope importable as a side-effect-free re-export for callers
// that want to inspect a single path without spawning a process.
export { classifyScope };

main();
