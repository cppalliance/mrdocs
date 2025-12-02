//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//
import { readFileSync } from "fs";
import path from "path";
import { evaluateDanger, type DangerInputs } from "./logic";

/**
 * Load a JSON fixture from disk and parse into DangerInputs.
 *
 * @param fixturePath path to the fixture file (absolute or relative).
 * @returns parsed DangerInputs used to simulate a PR.
 */
function loadFixture(fixturePath: string): DangerInputs {
    const resolved = path.isAbsolute(fixturePath) ? fixturePath : path.join(process.cwd(), fixturePath);
    const raw = readFileSync(resolved, "utf8");
    return JSON.parse(raw) as DangerInputs;
}

/**
 * Print a human-readable report for local runs.
 *
 * @param result evaluated Danger outputs to render.
 */
function printResult(result: ReturnType<typeof evaluateDanger>): void {
    console.log(result.summary.markdown);
    if (result.summary.highlights.length > 0) {
        console.log("\nHighlights:");
        for (const note of result.summary.highlights) {
            console.log(`- ${note}`);
        }
    }
    if (result.warnings.length > 0) {
        console.log("\nWarnings:");
        for (const message of result.warnings) {
            console.log(`- ${message}`);
        }
    }
}

/**
 * CLI entry: load a fixture (default sample) and print the evaluation summary.
 *
 * @remarks
 * This avoids GitHub calls so rule changes can be iterated locally.
 */
function main(): void {
    const fixtureArg = process.argv[2] || path.join(__dirname, "fixtures/sample-pr.json");
    const inputs = loadFixture(fixtureArg);
    const result = evaluateDanger(inputs);
    printResult(result);
}

main();
