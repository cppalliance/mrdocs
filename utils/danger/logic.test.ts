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
    affectsBuildPipeline,
    aggregateSizeWarnings,
    commitSizeInfos,
    evaluateDanger,
    expectedBodyLength,
    isCodeChange,
    parseCommitSummary,
    parsePrTemplateSections,
    prTemplateInfos,
    basicChecks,
    scopesTouched,
    summarizeScopes,
    validateCommits,
    type CommitInfo,
    type DangerInputs,
} from "./logic";

const sampleTemplate = [
    "<!-- Fill this in yourself. -->",
    "",
    "_What this PR does and why._",
    "",
    "## Changes",
    "",
    "_Replace the lines that apply._",
    "",
    "## Testing",
    "",
    "_How this change stays tested._",
    "",
    "## Documentation",
    "",
    "_What was updated._",
].join("\n");

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
            { filename: "tests/golden/fixtures/golden-tests/out.xml", additions: 100, deletions: 0 },
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
            { filename: "utils/bootstrap/src/installer.py", additions: 50, deletions: 10 },
            { filename: "utils/bootstrap/tests/test_installer.py", additions: 200, deletions: 0 },
            { filename: "utils/bootstrap/main.py", additions: 5, deletions: 2 },
            { filename: "CMakeLists.txt", additions: 3, deletions: 1 },
        ]);

        expect(report.totals.toolchain.files).toBe(2);
        expect(report.totals["toolchain-tests"].files).toBe(1);
        expect(report.totals.build.files).toBe(1);
        expect(report.totals.toolchain.additions).toBe(55);
        expect(report.totals["toolchain-tests"].additions).toBe(200);
    });
});

describe("aggregateSizeWarnings", () => {
    // Stays quiet when source churn is under the aggregate threshold.
    it("does not warn under the aggregate threshold", () => {
        const summary = summarizeScopes([
            { filename: "src/lib/file.cpp", additions: 1000, deletions: 500 },
        ]);
        expect(aggregateSizeWarnings(summary)).toEqual([]);
    });

    // Fires once aggregate source churn crosses the limit, even with well-sliced commits.
    it("warns when aggregate source churn exceeds the threshold", () => {
        const summary = summarizeScopes([
            { filename: "src/lib/a.cpp", additions: 3000, deletions: 0 },
            { filename: "src/lib/b.cpp", additions: 2500, deletions: 0 },
        ]);
        const warnings = aggregateSizeWarnings(summary);
        expect(warnings.length).toBe(1);
        expect(warnings[0]).toContain("5500");
    });

    // Ignores test, golden-test, and docs churn — only source counts.
    it("ignores non-source churn", () => {
        const summary = summarizeScopes([
            { filename: "tests/golden/fixtures/golden-tests/big.xml", additions: 20000, deletions: 0 },
            { filename: "docs/big.adoc", additions: 5000, deletions: 0 },
            { filename: "src/lib/small.cpp", additions: 10, deletions: 0 },
        ]);
        expect(aggregateSizeWarnings(summary)).toEqual([]);
    });
});

describe("expectedBodyLength", () => {
    // Bottoms out at 80 chars (log2(2) * 80) for tiny or zero-churn diffs.
    it("bottoms out at 80 chars for tiny changes", () => {
        expect(expectedBodyLength(0)).toBe(80);
        expect(expectedBodyLength(1)).toBe(80);
        expect(expectedBodyLength(2)).toBe(80);
    });

    // Grows roughly logarithmically with churn — 30k lines should still be only a few hundred chars.
    it("grows logarithmically with churn", () => {
        expect(expectedBodyLength(30000)).toBeGreaterThan(expectedBodyLength(1000));
        expect(expectedBodyLength(30000)).toBeLessThan(expectedBodyLength(1000) * 3);
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
                    { filename: "tests/golden/fixtures/golden-tests/out.xml", additions: 1000, deletions: 0 },
                ],
            },
        ];
        const infos = commitSizeInfos(commits);
        expect(infos.length).toBe(1);
    });
});

describe("scopesTouched", () => {
    // Classifies a list of file paths into the unique set of scopes touched.
    it("returns the unique set of scopes for the given paths", () => {
        const scopes = scopesTouched([
            "src/lib/file.cpp",
            "include/mrdocs/example.hpp",
            "tests/golden/fixtures/golden-tests/out.xml",
            "docs/index.adoc",
            ".github/workflows/ci.yml",
        ]);
        expect(scopes).toEqual(new Set(["source", "golden-tests", "docs", "ci"]));
    });

    // Empty input → empty set.
    it("returns an empty set when no paths are given", () => {
        expect(scopesTouched([])).toEqual(new Set());
    });

    // examples/** is its own scope; it does not get rolled into source.
    it("classifies paths under examples/ as the examples scope", () => {
        const scopes = scopesTouched([
            "examples/library/breaking-changes/src/main.cpp",
            "examples/configuration/inputs/extra-includes/docs/mrdocs.yml",
            "examples/generators/data-driven/md/simple.cpp",
        ]);
        expect(scopes).toEqual(new Set(["examples"]));
    });
});

describe("isCodeChange", () => {
    // Scopes that can affect the mrdocs binary trigger the full build matrix.
    it("is true when source is touched", () => {
        expect(isCodeChange(new Set(["source"]))).toBe(true);
    });

    it("is true when tests are touched", () => {
        expect(isCodeChange(new Set(["tests"]))).toBe(true);
    });

    it("is true when golden-tests are touched", () => {
        expect(isCodeChange(new Set(["golden-tests"]))).toBe(true);
    });

    it("is true when build files are touched", () => {
        expect(isCodeChange(new Set(["build"]))).toBe(true);
    });

    it("is true when third-party is touched", () => {
        expect(isCodeChange(new Set(["third-party"]))).toBe(true);
    });

    it("is true when examples are touched", () => {
        expect(isCodeChange(new Set(["examples"]))).toBe(true);
    });

    // Meta scopes do not justify rebuilding the binary.
    it("is false for docs-only changes", () => {
        expect(isCodeChange(new Set(["docs"]))).toBe(false);
    });

    it("is false for ci-only changes", () => {
        expect(isCodeChange(new Set(["ci"]))).toBe(false);
    });

    it("is false for tooling-only changes", () => {
        expect(isCodeChange(new Set(["tooling"]))).toBe(false);
    });

    it("is false for toolchain or toolchain-tests changes", () => {
        expect(isCodeChange(new Set(["toolchain"]))).toBe(false);
        expect(isCodeChange(new Set(["toolchain-tests"]))).toBe(false);
    });

    // A mixed change with any code scope still trips the flag.
    it("is true when at least one scope is code-relevant", () => {
        expect(isCodeChange(new Set(["docs", "ci", "source"]))).toBe(true);
    });

    // Empty set: no change.
    it("is false on an empty scope set", () => {
        expect(isCodeChange(new Set())).toBe(false);
    });
});

describe("affectsBuildPipeline", () => {
    // CI workflow files that drive the matrix must trip the full-matrix gate
    // even when classified under the `ci` scope (which is not a code change).
    it("flags ci-build.yml changes", () => {
        expect(affectsBuildPipeline([".github/workflows/ci-build.yml"])).toBe(true);
    });

    it("flags any ci-*.yml workflow", () => {
        expect(affectsBuildPipeline([".github/workflows/ci-matrix.yml"])).toBe(true);
        expect(affectsBuildPipeline([".github/workflows/ci-release.yml"])).toBe(true);
        expect(affectsBuildPipeline([".github/workflows/ci-documentation.yml"])).toBe(true);
        expect(affectsBuildPipeline([".github/workflows/ci-publish.yml"])).toBe(true);
        expect(affectsBuildPipeline([".github/workflows/ci-scope-detector.yml"])).toBe(true);
        expect(affectsBuildPipeline([".github/workflows/ci.yml"])).toBe(true);
    });

    it("flags build / install / demo scripts", () => {
        expect(affectsBuildPipeline([".github/scripts/install-mrdocs-package.sh"])).toBe(true);
        expect(affectsBuildPipeline([".github/scripts/generate-demos.sh"])).toBe(true);
    });

    it("flags bootstrap entry and sources", () => {
        expect(affectsBuildPipeline(["bootstrap.py"])).toBe(true);
        expect(affectsBuildPipeline(["utils/bootstrap/main.py"])).toBe(true);
    });

    // Workflow files that only drive PR comments / Danger.js posting don't
    // affect the build pipeline; they're exercised by their own jobs.
    it("does not flag pr-target-checks.yml", () => {
        expect(affectsBuildPipeline([".github/workflows/pr-target-checks.yml"])).toBe(false);
    });

    // PR template, gitignore, license: meta files with no build effect.
    it("does not flag PR template or meta files", () => {
        expect(affectsBuildPipeline([".github/pull_request_template.md"])).toBe(false);
        expect(affectsBuildPipeline([".gitignore"])).toBe(false);
        expect(affectsBuildPipeline(["LICENSE.txt"])).toBe(false);
    });

    // Bootstrap tests are exercised in utility-tests on every PR, no full
    // matrix needed.
    it("does not flag bootstrap tests", () => {
        expect(affectsBuildPipeline(["utils/bootstrap/tests/test_installer.py"])).toBe(false);
    });

    // Danger.js source is exercised by utility-tests' vitest step.
    it("does not flag utils/danger changes", () => {
        expect(affectsBuildPipeline(["utils/danger/logic.ts"])).toBe(false);
        expect(affectsBuildPipeline(["utils/danger/dangerfile.ts"])).toBe(false);
    });

    // Source / docs / build / third-party are already covered by scope-based
    // `isCodeChange`; this helper is purely the build-pipeline supplement.
    it("does not flag normal source paths", () => {
        expect(affectsBuildPipeline(["src/lib/file.cpp"])).toBe(false);
        expect(affectsBuildPipeline(["docs/index.adoc"])).toBe(false);
    });

    it("returns false on empty input", () => {
        expect(affectsBuildPipeline([])).toBe(false);
    });

    // Normalizes Windows-style separators so backslashes don't bypass the gate.
    it("normalizes backslashes", () => {
        expect(affectsBuildPipeline([".github\\workflows\\ci-build.yml"])).toBe(true);
    });
});

describe("parsePrTemplateSections", () => {
    // Pulls H2 headers in document order; ignores intro paragraphs and comments.
    it("extracts H2 headers from the template", () => {
        expect(parsePrTemplateSections(sampleTemplate)).toEqual(["Changes", "Testing", "Documentation"]);
    });

    // No headers means no sections to enforce.
    it("returns an empty array when there are no headers", () => {
        expect(parsePrTemplateSections("just a paragraph\n\nno headers here")).toEqual([]);
    });

    // Handles any ATX heading level so future templates can mix H1/H2/H3.
    it("extracts headings of any ATX level", () => {
        const template = [
            "# Summary",
            "## Changes",
            "### Subsection",
            "#### Detail",
            "##### Deeper",
            "###### Deepest",
        ].join("\n");
        expect(parsePrTemplateSections(template)).toEqual([
            "Summary",
            "Changes",
            "Subsection",
            "Detail",
            "Deeper",
            "Deepest",
        ]);
    });

    // Strips the optional ATX closing run of `#` characters.
    it("strips trailing closing hashes from ATX headers", () => {
        expect(parsePrTemplateSections("## Changes ##\n### Testing ###")).toEqual(["Changes", "Testing"]);
    });

    // Dedupes by title (case-insensitive) so cross-level repeats don't double-flag.
    it("dedupes repeated titles across levels", () => {
        const template = ["## Notes", "### notes", "#### NOTES"].join("\n");
        expect(parsePrTemplateSections(template)).toEqual(["Notes"]);
    });
});

describe("prTemplateInfos", () => {
    // A body that includes every template header produces no info.
    it("stays quiet when all template sections are present", () => {
        const body = [
            "Rationale for the change.",
            "",
            "## Changes",
            "- Source: tweak.",
            "",
            "## Testing",
            "Ran the suite.",
            "",
            "## Documentation",
            "No user-facing change.",
        ].join("\n");
        expect(prTemplateInfos(body, ["Changes", "Testing", "Documentation"])).toEqual([]);
    });

    // Missing sections are listed in the single info message.
    it("lists missing sections", () => {
        const body = ["Rationale.", "", "## Changes", "- Source: tweak."].join("\n");
        const infos = prTemplateInfos(body, ["Changes", "Testing", "Documentation"]);
        expect(infos).toHaveLength(1);
        expect(infos[0]).toContain("**Testing**");
        expect(infos[0]).toContain("**Documentation**");
        expect(infos[0]).not.toContain("**Changes**");
    });

    // Header matching is case-insensitive so contributors don't trip over capitalization.
    it("matches headers case-insensitively", () => {
        const body = ["## changes", "## testing", "## documentation"].join("\n");
        expect(prTemplateInfos(body, ["Changes", "Testing", "Documentation"])).toEqual([]);
    });

    // The body can use a different heading level than the template — title alone is what matters.
    it("matches sections regardless of heading level", () => {
        const body = ["# Changes", "### Testing", "###### Documentation"].join("\n");
        expect(prTemplateInfos(body, ["Changes", "Testing", "Documentation"])).toEqual([]);
    });

    // An empty body is already covered by the empty-description warning — skip to avoid noise.
    it("skips when the PR body is empty", () => {
        expect(prTemplateInfos("", ["Changes", "Testing"])).toEqual([]);
        expect(prTemplateInfos("   \n\n  ", ["Changes", "Testing"])).toEqual([]);
    });

    // A template with no headers produces no info.
    it("skips when the template has no headers", () => {
        expect(prTemplateInfos("some body", [])).toEqual([]);
    });

    // Singular wording when exactly one section is missing.
    it("uses singular wording for a single missing section", () => {
        const body = ["## Changes", "## Testing"].join("\n");
        const infos = prTemplateInfos(body, ["Changes", "Testing", "Documentation"]);
        expect(infos[0]).toContain("missing template section:");
    });
});

describe("evaluateDanger pr-template info", () => {
    // The new check is wired into the infos channel so it renders as [!NOTE].
    it("surfaces missing template sections via infos", () => {
        const result = evaluateDanger({
            files: [{ filename: "src/lib/x.cpp", additions: 10, deletions: 1 }],
            commits: [{ sha: "ab", message: "fix: small change" }],
            prBody: "A short rationale that explains what is going on, with no template structure at all.",
            prTitle: "fix: small change",
            labels: [],
            prTemplate: sampleTemplate,
        });
        expect(result.infos.some((m) => m.includes("missing template sections"))).toBe(true);
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

    // Warns when a feat commit ships without any documentation update.
    it("warns when a feat commit ships without docs", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody: "Adds a shiny new generator option.\n\nTesting: ran golden tests locally.",
            prTitle: "feat: shiny option",
            labels: [],
        };

        const summary = summarizeScopes([
            { filename: "src/lib/Gen/option.cpp", additions: 30, deletions: 1 },
            { filename: "src/test/option.cpp", additions: 20, deletions: 0 },
        ]);
        const parsed = validateCommits([{ sha: "3", message: "feat: shiny option" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);

        expect(warnings.some((message) => message.includes("does not update any documentation"))).toBe(true);
    });

    // Stays quiet when a feat commit also touches docs.
    it("does not warn when a feat commit updates docs", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody: "Adds a shiny new generator option.\n\nTesting: ran golden tests.",
            prTitle: "feat: shiny option",
            labels: [],
        };

        const summary = summarizeScopes([
            { filename: "src/lib/Gen/option.cpp", additions: 30, deletions: 1 },
            { filename: "src/test/option.cpp", additions: 20, deletions: 0 },
            { filename: "docs/modules/ROOT/pages/options.adoc", additions: 12, deletions: 0 },
        ]);
        const parsed = validateCommits([{ sha: "4", message: "feat: shiny option" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);

        expect(warnings.some((message) => message.includes("does not update any documentation"))).toBe(false);
    });

    // Honors explicit opt-out labels.
    it("respects no-docs-needed label", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody: "Adds an internal-only feature flag.\n\nTesting: covered by existing suites.",
            prTitle: "feat: internal flag",
            labels: ["no-docs-needed"],
        };

        const summary = summarizeScopes([
            { filename: "src/lib/internal.cpp", additions: 5, deletions: 0 },
            { filename: "src/test/internal.cpp", additions: 5, deletions: 0 },
        ]);
        const parsed = validateCommits([{ sha: "5", message: "feat: internal flag" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);

        expect(warnings.some((message) => message.includes("does not update any documentation"))).toBe(false);
    });

    // Warns when the description is too short relative to the size of the change.
    it("warns when description is short relative to churn", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody:
                "Big refactor across the codebase. Tested locally with the existing suite; no behavior change expected.",
            prTitle: "refactor: massive",
            labels: [],
        };
        const summary = summarizeScopes([
            { filename: "src/lib/big.cpp", additions: 5000, deletions: 1000 },
        ]);
        const parsed = validateCommits([{ sha: "sx", message: "refactor: massive" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);
        expect(warnings.some((m) => m.includes("relative to the size of this change"))).toBe(true);
        expect(warnings.some((m) => m.includes("PR description looks empty"))).toBe(false);
    });

    // An unfilled PR template (italic placeholders + headers, no real content) must still trip
    // the empty-description warning and must not satisfy the testing-mention check via the
    // "## Testing" header.
    it("treats an unfilled PR template as empty after cleaning", () => {
        const unfilledTemplate = [
            "<!-- Fill this in yourself, or have your favorite AI agent draft it from the diff. -->",
            "",
            "## Summary",
            "",
            "_What this PR does and why. For large or non-trivial changes, link a design doc or issue, or explain the design inline._",
            "",
            "## Changes",
            "",
            "_Replace the lines that apply, delete the rest._",
            "",
            "- **Source**: _Implementation changes._",
            "- **Tests**: _New or updated unit tests and fixtures._",
            "- **Breaking changes**: _Anything downstream users need to know._",
            "",
            "## Testing",
            "",
            "_How this change stays tested going forward — the tests in this PR that cover the new behavior, plus any CI workflow changes needed so that coverage runs on every future build._",
            "",
            "## Documentation",
            "",
            "_What was updated in the documentation or why documentation is not needed._",
        ].join("\n");
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody: unfilledTemplate,
            prTitle: "feat: something",
            labels: [],
        };
        const summary = summarizeScopes([
            { filename: "src/lib/file.cpp", additions: 100, deletions: 0 },
        ]);
        const parsed = validateCommits([{ sha: "swt", message: "feat: something" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);
        expect(warnings.some((m) => m.includes("PR description looks empty"))).toBe(true);
        // The "## Testing" header must not satisfy the test-mention check on its own —
        // the source-without-tests warning should still fire.
        expect(warnings.some((m) => m.includes("Source changed"))).toBe(true);
    });

    // Strips HTML comments before measuring length so PR-template scaffolding cannot game the check.
    it("ignores HTML comments when measuring description length", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody:
                "<!-- This is a long template comment that should not count toward the body length. -->\n" +
                "<!-- More scaffolding here that adds many characters but conveys nothing useful. -->\n" +
                "Fix.",
            prTitle: "fix: tiny",
            labels: [],
        };
        const summary = summarizeScopes([
            { filename: "src/lib/tiny.cpp", additions: 1, deletions: 1 },
        ]);
        const parsed = validateCommits([{ sha: "sy", message: "fix: tiny" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);
        expect(warnings.some((m) => m.includes("PR description looks empty"))).toBe(true);
    });

    // Stays quiet when description length is well-matched to a small change.
    it("does not warn on short body for tiny diffs", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody: "Fix a typo in a code comment. Verified locally by running the existing unit test suite.",
            prTitle: "fix: typo",
            labels: [],
        };
        const summary = summarizeScopes([
            { filename: "src/lib/typo.cpp", additions: 1, deletions: 1 },
        ]);
        const parsed = validateCommits([{ sha: "sz", message: "fix: typo" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);
        expect(warnings.some((m) => m.includes("PR description looks empty"))).toBe(false);
        expect(warnings.some((m) => m.includes("relative to the size of this change"))).toBe(false);
    });

    // End-to-end: PR #1178-shaped change surfaces both the aggregate-size and short-description warnings.
    it("flags a large PR with a terse description through evaluateDanger", () => {
        const result = evaluateDanger({
            files: [
                { filename: "src/lib/a.cpp", additions: 3000, deletions: 0 },
                { filename: "src/lib/b.cpp", additions: 3000, deletions: 0 },
                { filename: "tests/golden/fixtures/golden-tests/x.xml", additions: 10000, deletions: 0 },
                { filename: "docs/option.adoc", additions: 5, deletions: 0 },
            ],
            commits: [{ sha: "ab", message: "feat: large change" }],
            prBody:
                "Adds a new schema-generation option to mrdocs. Tested locally against the golden test suite and verified the generated schemas render correctly.",
            prTitle: "feat: large change",
            labels: [],
        });
        expect(result.warnings.some((m) => m.includes("source lines"))).toBe(true);
        expect(result.warnings.some((m) => m.includes("relative to the size of this change"))).toBe(true);
    });

    // Stays quiet for non-feature commits even without docs.
    it("does not warn for fix commits without docs", () => {
        const inputs: DangerInputs = {
            files: [],
            commits: [],
            prBody: "Fixes off-by-one.\n\nTesting: added a regression unit test.",
            prTitle: "fix: off-by-one",
            labels: [],
        };

        const summary = summarizeScopes([
            { filename: "src/lib/loop.cpp", additions: 2, deletions: 2 },
            { filename: "src/test/loop.cpp", additions: 8, deletions: 0 },
        ]);
        const parsed = validateCommits([{ sha: "6", message: "fix: off-by-one" }]).parsed;
        const warnings = basicChecks(inputs, summary, parsed);

        expect(warnings.some((message) => message.includes("does not update any documentation"))).toBe(false);
    });
});
