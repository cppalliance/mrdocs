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
            { filename: "SourceFileNames.cpp", additions: 1, deletions: 0 },
            { filename: ".clang-format", additions: 0, deletions: 0 },
            { filename: ".gitignore", additions: 0, deletions: 0 },
            { filename: "LICENSE.txt", additions: 0, deletions: 0 },
        ]);

        expect(report.totals.source.files).toBe(2);
        expect(report.totals.tests.files).toBe(1);
        expect(report.totals["golden-tests"].files).toBe(1);
        expect(report.totals.docs.files).toBe(1);
        expect(report.totals.tooling.files).toBe(1);
        expect(report.totals.ci.files).toBe(2);
        expect(report.overall.files).toBe(8);
    });

    it("separates toolchain source from toolchain tests", () => {
        const report = summarizeScopes([
            { filename: "util/bootstrap/src/installer.py", additions: 50, deletions: 10 },
            { filename: "util/bootstrap/tests/test_installer.py", additions: 200, deletions: 0 },
            { filename: "util/bootstrap/main.py", additions: 5, deletions: 2 },
            { filename: "CMakeLists.txt", additions: 3, deletions: 1 },
        ]);

        expect(report.totals.toolchain.files).toBe(2);
        expect(report.totals["toolchain-tests"].files).toBe(1);
        expect(report.totals.build.files).toBe(1);
        expect(report.totals.toolchain.additions).toBe(55);
        expect(report.totals["toolchain-tests"].additions).toBe(200);
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

    // Ensures refactor-only work does not nag for tests when the change is mechanical.
    it("skips test warning for refactor commits", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody: "Refactor clean-up.\n\nTesting: relies on existing coverage; no behavior change.",
            prTitle: "refactor: tidy includes",
            labels: [],
        };

        const summary = summarizeScopes([{ filename: "src/lib/refactor.cpp", additions: 10, deletions: 2 }]);
        const parsed = validateCommits([{ sha: "2", message: "refactor: tidy includes" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);

        expect(warnings.some((message) => message.includes("Source changed"))).toBe(false);
    });
});
