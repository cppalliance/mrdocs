//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//
import { describe, expect, it } from "vitest";
import { renderDangerReport } from "./format";
import { summarizeScopes, type DangerResult } from "./logic";

describe("renderDangerReport", () => {
    it("puts warnings at the top as GitHub admonitions", () => {
        const summary = summarizeScopes([
            { filename: "src/lib/example.cpp", additions: 10, deletions: 2 },
            { filename: "docs/index.adoc", additions: 1, deletions: 0 },
        ]);
        const result: DangerResult = {
            warnings: ["First issue", "Second issue"],
            infos: [],
            summary,
        };

        const output = renderDangerReport(result);

        expect(output.startsWith("> 🚧 Danger.js checks for MrDocs")).toBe(true);
        expect(output).toContain("## ⚠️ Warnings");
        expect((output.match(/> \[!WARNING\]/g) || []).length).toBe(2);
        expect(output).toContain("## 🧾 Changes by Scope");
        expect(output).toMatch(/\|\s*\*\*Total\*\*\s*\|/);
        expect(output).toContain("Legend: Files + (added), Files ~ (modified), Files ↔ (renamed), Files - (removed)");
        expect(output).toContain("## 🔝 Top Files");
    });

    it("renders informational notes separately from warnings", () => {
        const summary = summarizeScopes([{ filename: "src/lib/example.cpp", additions: 1, deletions: 0 }]);
        const result: DangerResult = { warnings: [], infos: ["Large commit"], summary };

        const output = renderDangerReport(result);

        expect(output).toContain("## ℹ️ Info");
        expect(output).toContain("[!NOTE]");
        expect(output).toContain("Large commit");
    });

    it("formats scope totals with bold metrics and consistent churn", () => {
        const summary = summarizeScopes([
            { filename: "src/lib/example.cpp", additions: 3, deletions: 1 },
            { filename: "src/test/example_test.cpp", additions: 2, deletions: 0 },
        ]);
        const result: DangerResult = { warnings: [], infos: [], summary };

        const output = renderDangerReport(result);

        expect(output).toMatch(/\|\s*Source\s*\|\s*\*\*4\*\*\s*\|\s*3\s*\|\s*1\s*\|\s*\*\*1\*\*\s*\|\s*-\s*\|\s*1\s*\|\s*-\s*\|\s*-\s*\|/);
        expect(output).toMatch(/\|\s*Tests\s*\|\s*\*\*2\*\*\s*\|\s*2\s*\|\s*-\s*\|\s*\*\*1\*\*\s*\|\s*-\s*\|\s*1\s*\|\s*-\s*\|\s*-\s*\|/);
        expect(output).toMatch(/\|\s*\*\*Total\*\*\s*\|\s*\*\*6\*\*\s*\|\s*5\s*\|\s*1\s*\|\s*\*\*2\*\*\s*\|/);
        expect(output).toContain("## ✨ Highlights");
        expect(output.trim().startsWith("> 🚧 Danger.js checks for MrDocs")).toBe(true);
    });

    it("treats removed files as positive file deltas", () => {
        const summary = summarizeScopes([
            { filename: "src/lib/old.cpp", additions: 0, deletions: 5, status: "removed" },
        ]);
        const result: DangerResult = { warnings: [], infos: [], summary };

        const output = renderDangerReport(result);
        const sourceRow = output.split("\n").find((line) => line.startsWith("| Source"));

        expect(sourceRow).toBeDefined();
        expect(sourceRow).not.toMatch(/-1/);
        expect(sourceRow).toMatch(
            /\|\s*Source\s*\|\s*\*\*5\*\*\s*\|\s*-\s*\|\s*5\s*\|\s*\*\*1\*\*\s*\|\s*-\s*\|\s*-\s*\|\s*-\s*\|\s*1\s*\|/,
        );
    });
});
