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
 * Semantic areas of the repository used to group diff churn in reports and rules.
 */
type ScopeKey =
    | "golden-tests"
    | "tests"
    | "source"
    | "docs"
    | "ci"
    | "build"
    | "tooling"
    | "third-party"
    | "other";

/**
 * Minimal file change metadata used for scope aggregation and churn counts.
 */
export interface FileChange {
    filename: string;
    additions: number;
    deletions: number;
    status?: string;
}

/**
 * Commit metadata including message and optional per-file stats for size checks.
 */
export interface CommitInfo {
    sha: string;
    message: string;
    files?: FileChange[];
}

/**
 * Parsed Conventional Commit first-line details extracted from a commit message.
 */
export interface ParsedCommit {
    sha: string;
    summary: string;
    type?: string;
    scope?: string;
    subject?: string;
}

/**
 * Accumulated churn totals for a single scope.
 */
export interface ScopeTotals {
    scope: ScopeKey;
    files: number;
    additions: number;
    deletions: number;
}

/**
 * Scope summary output that is rendered into a Markdown table.
 */
export interface ScopeReport {
    totals: Record<ScopeKey, ScopeTotals>;
    overall: { files: number; additions: number; deletions: number };
    markdown: string;
    highlights: string[];
}

/**
 * Data pulled from the PR that the rules evaluate.
 */
export interface DangerInputs {
    files: FileChange[];
    commits: CommitInfo[];
    prBody: string;
    prTitle: string;
    labels: string[];
}

/**
 * Final result with warnings and the rendered scope summary.
 */
export interface DangerResult {
    warnings: string[];
    summary: ScopeReport;
}

const allowedTypes = [
    "feat",
    "fix",
    "docs",
    "style",
    "refactor",
    "perf",
    "test",
    "build",
    "ci",
    "chore",
    "revert",
];

const scopeFormat = /^[a-z0-9._/-]+$/i;
const typeSet = new Set(allowedTypes);
const skipTestLabels = new Set(["no-tests-needed", "skip-tests", "tests-not-required"]);
const skipTestMarkers = ["[skip danger tests]", "[danger skip tests]"];
const nonTestCommitLimit = 800;

interface ScopeRule {
    scope: ScopeKey;
    patterns: RegExp[];
}

const scopeRules: ScopeRule[] = [
    { scope: "golden-tests", patterns: [/^test-files\/golden-tests\//i] },
    {
        scope: "tests",
        patterns: [
            /^src\/test\//i,
            /^src\/test_suite\//i,
            /^test-files\//i,
            /^docs\/website\/snippets\//i,
        ],
    },
    {
        scope: "source",
        patterns: [/^src\//i, /^include\//i, /^examples\//i, /^share\//i],
    },
    { scope: "docs", patterns: [/^docs\//i, /^README\.adoc$/i, /^Doxyfile/i] },
    { scope: "ci", patterns: [/^\.github\//, /^\.roadmap\//] },
    {
        scope: "build",
        patterns: [
            /^CMakeLists\.txt$/i,
            /^CMakePresets\.json$/i,
            /^CMakeUserPresets\.json/i,
            /^CMakeUserPresets\.json\.example/i,
            /^install\//i,
            /^bootstrap\.py$/i,
            /^mrdocs\.rnc$/i,
            /^mrdocs-config\.cmake\.in$/i,
        ],
    },
    { scope: "tooling", patterns: [/^util\//i, /^tools\//i] },
    { scope: "third-party", patterns: [/^third-party\//i] },
];

/**
 * Normalize a file path for consistent scope matching.
 * Converts Windows separators to POSIX to make regex checks reliable.
 */
function normalizePath(path: string): string {
    return path.replace(/\\/g, "/");
}

/**
 * Map a file path to its logical scope based on repository layout.
 *
 * @param path raw file path from GitHub.
 * @returns matched ScopeKey or "other" if no rules match.
 */
function getScope(path: string): ScopeKey {
    const normalized = normalizePath(path);
    for (const rule of scopeRules) {
        if (rule.patterns.some((pattern) => pattern.test(normalized))) {
            return rule.scope;
        }
    }
    return "other";
}

/**
 * Aggregate file-level churn into scope totals and produce a Markdown summary table.
 *
 * @param files changed files with add/delete counts.
 * @returns ScopeReport containing totals, markdown table, and highlight notes.
 */
export function summarizeScopes(files: FileChange[]): ScopeReport {
    const totals: Record<ScopeKey, ScopeTotals> = {
        "golden-tests": { scope: "golden-tests", files: 0, additions: 0, deletions: 0 },
        tests: { scope: "tests", files: 0, additions: 0, deletions: 0 },
        source: { scope: "source", files: 0, additions: 0, deletions: 0 },
        docs: { scope: "docs", files: 0, additions: 0, deletions: 0 },
        ci: { scope: "ci", files: 0, additions: 0, deletions: 0 },
        build: { scope: "build", files: 0, additions: 0, deletions: 0 },
        tooling: { scope: "tooling", files: 0, additions: 0, deletions: 0 },
        "third-party": { scope: "third-party", files: 0, additions: 0, deletions: 0 },
        other: { scope: "other", files: 0, additions: 0, deletions: 0 },
    };

    let fileCount = 0;
    let additions = 0;
    let deletions = 0;

    for (const file of files) {
        const scope = getScope(file.filename);
        totals[scope].files += 1;
        totals[scope].additions += file.additions || 0;
        totals[scope].deletions += file.deletions || 0;
        fileCount += 1;
        additions += file.additions || 0;
        deletions += file.deletions || 0;
    }

    const scopesInOrder: ScopeKey[] = [
        "source",
        "tests",
        "golden-tests",
        "docs",
        "ci",
        "build",
        "tooling",
        "third-party",
        "other",
    ];

    const nonEmptyScopes = scopesInOrder.filter((scope) => totals[scope].files > 0);
    const header = "| Scope | Files | + / - |\n| --- | ---: | ---: |\n";
    const rows =
        nonEmptyScopes
            .map((scope) => {
                const scoped = totals[scope];
                return `| ${scope} | ${scoped.files} | +${scoped.additions} / -${scoped.deletions} |`;
            })
            .join("\n") || "| (no changes) | 0 | +0 / -0 |";

    const highlights = [];
    if (totals["golden-tests"].files > 0) {
        highlights.push("Golden test fixtures changed");
    }
    if (totals.tests.files === 0 && totals.source.files > 0) {
        highlights.push("Source updated without test coverage changes");
    }

    const markdown = [
        "### Change summary by scope",
        `- Files changed: ${fileCount}`,
        `- Total churn: +${additions} / -${deletions}`,
        "",
        header + rows,
    ].join("\n");

    return {
        totals,
        overall: { files: fileCount, additions, deletions },
        markdown,
        highlights,
    };
}

/**
 * Parse the first line of a commit message using a light Conventional Commit rule.
 *
 * @param summary first line of the commit message.
 * @returns ParsedCommit when format matches, otherwise null.
 */
export function parseCommitSummary(summary: string): ParsedCommit | null {
    const match = summary.match(/^(\w+)(?:\(([^)]+)\))?:\s+(.+)$/);
    if (!match) {
        return null;
    }
    const [, type, scope, subject] = match;
    if (scope && !scopeFormat.test(scope)) {
        return null;
    }
    return { sha: "", summary, type, scope, subject };
}

/**
 * Validate commit first-line formatting and allowed types.
 *
 * @param commits list of commits to validate.
 * @returns warnings and the parsed commit metadata for downstream checks.
 */
export function validateCommits(commits: CommitInfo[]): { warnings: string[]; parsed: ParsedCommit[] } {
    const warnings: string[] = [];
    const parsed: ParsedCommit[] = [];
    const badFormat: string[] = [];
    const badType: string[] = [];

    for (const commit of commits) {
        const summary = commit.message.split("\n")[0].trim();
        const parsedLine = parseCommitSummary(summary);
        if (!parsedLine) {
            // === Conventional Commit format warnings ===
            badFormat.push(`\`${summary}\``);
            parsed.push({ sha: commit.sha, summary });
            continue;
        }

        const entry: ParsedCommit = { ...parsedLine, sha: commit.sha, summary };
        parsed.push(entry);

        if (!typeSet.has(parsedLine.type || "")) {
            // === Conventional Commit type validation warnings ===
            badType.push(`\`${summary}\``);
        }
    }

    if (badFormat.length > 0) {
        warnings.push(
            [
                "Some commits do not follow Conventional Commit first-line formatting (`type(scope): subject`):",
                badFormat.join(", "),
                "Examples: `fix: adjust docs link`, `feat(api): allow overrides`.",
            ].join(" "),
        );
    }

    if (badType.length > 0) {
        warnings.push(
            [
                `Commit types must be one of ${allowedTypes.join(", ")}.`,
                "These commits need attention:",
                badType.join(", "),
            ].join(" "),
        );
    }

    return { warnings, parsed };
}

/**
 * Warn when a single commit changes too many non-test lines to encourage smaller slices.
 *
 * @param commits commits with per-file stats.
 * @returns warning messages for commits that exceed the threshold.
 */
export function commitSizeWarnings(commits: CommitInfo[]): string[] {
    const messages: string[] = [];
    for (const commit of commits) {
        if (!commit.files || commit.files.length === 0) {
            continue;
        }

        let churn = 0;
        for (const file of commit.files) {
            const scope = getScope(file.filename);
            if (scope === "tests" || scope === "golden-tests") {
                continue;
            }
            churn += (file.additions || 0) + (file.deletions || 0);
        }

        if (churn > nonTestCommitLimit) {
            const shortSha = commit.sha.substring(0, 7);
            // === Commit size warnings (non-test churn) ===
            messages.push(
                `Commit \`${shortSha}\` changes ${churn} non-test lines. Consider splitting it into smaller, reviewable chunks.`,
            );
        }
    }
    return messages;
}

/**
 * Check for explicit signals to skip source-vs-test warnings.
 *
 * @param prBody pull request body text.
 * @param labels labels applied to the pull request.
 * @returns true when skip markers or labels are present.
 */
function hasSkipTests(prBody: string, labels: string[]): boolean {
    if (labels.some((label) => skipTestLabels.has(label))) {
        return true;
    }
    const body = prBody.toLowerCase();
    return skipTestMarkers.some((marker) => body.includes(marker));
}

/**
 * Additional hygiene checks around PR description, test coverage signals, and coherence.
 *
 * @param input PR metadata and labels.
 * @param scopes aggregated scope summary.
 * @param parsedCommits parsed commit metadata for type checks.
 * @returns warning messages for hygiene gaps.
 */
export function basicChecks(input: DangerInputs, scopes: ScopeReport, parsedCommits: ParsedCommit[]): string[] {
    const warnings: string[] = [];

    const cleanedBody = (input.prBody || "").trim();
    if (cleanedBody.length < 40) {
        // === PR description completeness warnings ===
        warnings.push("PR description looks empty. Please add a short rationale and testing notes.");
    } else if (!/test(ed|ing)?/i.test(cleanedBody)) {
        // === Missing testing notes warnings ===
        warnings.push("Add a brief note about how this change was tested (or why tests are not needed).");
    }

    const skipTests = hasSkipTests(input.prBody || "", input.labels);
    if (
        !skipTests &&
        scopes.totals.source.files > 0 &&
        scopes.totals.tests.files === 0 &&
        scopes.totals["golden-tests"].files === 0
    ) {
        // === Source changes without tests/fixtures warnings ===
        warnings.push(
            "Source changed but no tests or fixtures were updated. Add coverage or label with `no-tests-needed` / `[skip danger tests]` when appropriate.",
        );
    }

    const commitTypes = new Set(parsedCommits.map((commit) => commit.type).filter(Boolean) as string[]);
    const totalFiles = Object.values(scopes.totals).reduce((sum, scope) => sum + scope.files, 0);
    const nonDocFiles = totalFiles - scopes.totals.docs.files;
    const testFiles = scopes.totals.tests.files + scopes.totals["golden-tests"].files;
    const nonTestFiles = totalFiles - testFiles;

    const docOnlyChange =
        scopes.totals.docs.files > 0 &&
        nonDocFiles === 0;

    if (docOnlyChange && ["feat", "fix", "refactor", "perf"].some((type) => commitTypes.has(type))) {
        // === Docs-only change with feature/fix commit types warnings ===
        warnings.push("Commits look like feature/fix work, but the PR only changes docs. Double-check commit types.");
    }

    const testsOnlyChange = testFiles > 0 && nonTestFiles === 0;

    if (testsOnlyChange && ["feat", "fix"].some((type) => commitTypes.has(type))) {
        // === Tests-only change with feature/fix commit types warnings ===
        warnings.push("Commits are marked as feature/fix, but the diff only touches tests. Consider a `test:` type instead.");
    }

    return warnings;
}

/**
 * Entry point: evaluate PR inputs and return scope summary plus warning messages.
 *
 * @param input files, commits, labels, and PR text gathered by the runner.
 * @returns warnings and a scope summary for rendering in Danger.
 */
export function evaluateDanger(input: DangerInputs): DangerResult {
    const summary = summarizeScopes(input.files);
    const commitValidation = validateCommits(input.commits);

    const warnings = [
        ...commitValidation.warnings,
        ...commitSizeWarnings(input.commits),
        ...basicChecks(input, summary, commitValidation.parsed),
    ];

    return { warnings, summary };
}
