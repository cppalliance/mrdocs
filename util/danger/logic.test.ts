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
import {
    commitSizeInfos,
    parseCommitSummary,
    basicChecks,
    summarizeScopes,
    validateCommits,
    type CommitInfo,
    type DangerInputs,
} from "./logic";

describe("parseCommitSummary", () => {
    // Ensures we correctly extract type, scope, and subject when format is valid.
    it("parses valid commit summaries", () => {
        const parsed = parseCommitSummary("fix(core): handle edge case");
        expect(parsed?.type).toBe("fix");
        expect(parsed?.scope).toBe("core");
        expect(parsed?.subject).toBe("handle edge case");
    });

    // Guards against accepting malformed commit first lines.
    it("rejects invalid format", () => {
        expect(parseCommitSummary("invalid summary")).toBeNull();
    });
});

describe("summarizeScopes", () => {
    // Verifies file paths are bucketed into the correct scopes and totals are tallied.
    it("aggregates by scope", () => {
        const report = summarizeScopes([
            { filename: "src/lib/file.cpp", additions: 10, deletions: 2 },
            { filename: "src/test/file.cpp", additions: 5, deletions: 1 },
            { filename: "test-files/golden-tests/out.xml", additions: 100, deletions: 0 },
            { filename: "docs/index.adoc", additions: 4, deletions: 0 },
        ]);

        expect(report.totals.source.files).toBe(1);
        expect(report.totals.tests.files).toBe(1);
        expect(report.totals["golden-tests"].files).toBe(1);
        expect(report.totals.docs.files).toBe(1);
        expect(report.overall.files).toBe(4);
    });
});

describe("commitSizeInfos", () => {
    // Confirms that large non-test churn emits an informational note while ignoring test fixtures.
    it("flags large non-test commits", () => {
        const commits: CommitInfo[] = [
            {
                sha: "abc",
                message: "feat: huge change",
                files: [
                    { filename: "src/lib/large.cpp", additions: 1800, deletions: 400 },
                    { filename: "test-files/golden-tests/out.xml", additions: 1000, deletions: 0 },
                ],
            },
        ];
        const infos = commitSizeInfos(commits);
        expect(infos.length).toBe(1);
    });
});

describe("starterChecks", () => {
    // Checks that source changes without accompanying tests produce a warning.
    it("requests tests when source changes without coverage", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody: "Summary\n\nTesting: pending",
            prTitle: "Test PR",
            labels: [],
        };
        const summary = summarizeScopes([{ filename: "src/lib/file.cpp", additions: 1, deletions: 0 }]);
        const parsed = validateCommits([{ sha: "1", message: "fix: change" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);
        expect(warnings.some((message) => message.includes("Source changed"))).toBe(true);
    });
});
