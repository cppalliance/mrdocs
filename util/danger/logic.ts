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
export type ScopeKey =
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
    status: { added: number; modified: number; removed: number; renamed: number; other: number };
}

export interface FileSummary {
    filename: string;
    scope: ScopeKey;
    additions: number;
    deletions: number;
    churn: number;
    status?: string;
}

/**
 * Scope summary output that is rendered into a Markdown table.
 */
export interface ScopeReport {
    totals: Record<ScopeKey, ScopeTotals>;
    overall: {
        files: number;
        additions: number;
        deletions: number;
        status: { added: number; modified: number; removed: number; renamed: number; other: number };
    };
    topFiles: FileSummary[];
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
    infos: string[];
    summary: ScopeReport;
}

/** Display order for scopes in rendered reports. */
export const scopeDisplayOrder: ScopeKey[] = [
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
const nonTestCommitLimit = 2000;

/**
 * Format churn as a + / - pair with explicit signs.
 */
export function formatChurn(additions: number, deletions: number): string {
    return `+${additions} / -${deletions}`;
}

/**
 * Normalize GitHub file status into summarized buckets.
 */
function normalizeStatus(status?: string): "added" | "modified" | "removed" | "renamed" | "other" {
    switch ((status || "").toLowerCase()) {
        case "added":
            return "added";
        case "removed":
            return "removed";
        case "renamed":
            return "renamed";
        case "modified":
        case "changed":
        case "copied":
            return "modified";
        default:
            return "modified";
    }
}

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
        patterns: [/^src\//i, /^include\//i, /^examples\//i, /^share\//i, /^SourceFileNames\.cpp$/i],
    },
    { scope: "docs", patterns: [/^docs\//i, /^README\.adoc$/i, /^Doxyfile/i] },
    { scope: "ci", patterns: [/^\.github\//, /^\.roadmap\//, /^\.gitignore$/i, /^\.gitattributes$/i, /^LICENSE\.txt$/i] },
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
    { scope: "tooling", patterns: [/^tools\//i, /^util\/(?!danger\/)/i] },
    { scope: "tooling", patterns: [/^\.clang-format$/i] },
    { scope: "ci", patterns: [/^util\/danger\//i, /^\.github\//, /^\.roadmap\//] },
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
export function classifyScope(path: string): ScopeKey {
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
        "golden-tests": {
            scope: "golden-tests",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
        tests: {
            scope: "tests",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
        source: {
            scope: "source",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
        docs: {
            scope: "docs",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
        ci: {
            scope: "ci",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
        build: {
            scope: "build",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
        tooling: {
            scope: "tooling",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
        "third-party": {
            scope: "third-party",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
        other: {
            scope: "other",
            files: 0,
            additions: 0,
            deletions: 0,
            status: { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 },
        },
    };

    let fileCount = 0;
    let additions = 0;
    let deletions = 0;
    const statusTotals = { added: 0, modified: 0, removed: 0, renamed: 0, other: 0 };
    const fileSummaries: FileSummary[] = [];

    for (const file of files) {
        const scope = classifyScope(file.filename);
        totals[scope].files += 1;
        totals[scope].additions += file.additions || 0;
        totals[scope].deletions += file.deletions || 0;
        fileCount += 1;
        additions += file.additions || 0;
        deletions += file.deletions || 0;

        const normStatus = normalizeStatus(file.status);
        statusTotals[normStatus] += 1;
        totals[scope].status[normStatus] += 1;

        fileSummaries.push({
            filename: file.filename,
            scope,
            additions: file.additions || 0,
            deletions: file.deletions || 0,
            churn: (file.additions || 0) + (file.deletions || 0),
            status: file.status,
        });
    }

    const nonEmptyScopes = scopeDisplayOrder.filter((scope) => totals[scope].files > 0);
    const header = ["| Scope | Files | + / - |", "| --- | ---: | ---: |"].join("\n");
    const rows =
        nonEmptyScopes
            .map((scope) => {
                const scoped = totals[scope];
                return `| ${scope} | **${scoped.files}** | **${formatChurn(scoped.additions, scoped.deletions)}** |`;
            })
            .join("\n") || "| (no changes) | **0** | **+0 / -0** |";

    const highlights: string[] = [];
    const goldenStatus = totals["golden-tests"].status;
    if (goldenStatus.modified > 0 || goldenStatus.renamed > 0) {
        highlights.push("Existing golden tests changed (behavior likely shifted)");
    } else if (goldenStatus.added > 0) {
        highlights.push("New golden tests added");
    } else if (goldenStatus.removed > 0) {
        highlights.push("Golden tests removed");
    }

    const markdown = [header, rows].join("\n");

    return {
        totals,
        overall: { files: fileCount, additions, deletions, status: statusTotals },
        topFiles: fileSummaries
            .sort((a, b) => b.churn - a.churn || a.filename.localeCompare(b.filename))
            .slice(0, 3),
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
export function commitSizeInfos(commits: CommitInfo[]): string[] {
    const messages: string[] = [];
    for (const commit of commits) {
        if (!commit.files || commit.files.length === 0) {
            continue;
        }

        let churn = 0;
        for (const file of commit.files) {
            const scope = classifyScope(file.filename);
            if (scope !== "source") {
                continue;
            }
            churn += (file.additions || 0) + (file.deletions || 0);
        }

        const summary = commit.message.split("\n")[0].trim();
        const parsedType = parseCommitSummary(summary || "")?.type;

        if (churn > nonTestCommitLimit && parsedType !== "refactor") {
            const shortSha = commit.sha.substring(0, 7);
            // === Commit size informational notes (non-test churn) ===
            messages.push(
                `Commit \`${shortSha}\` (${summary}) touches ${churn} source lines (non-test). Large change; add reviewer context if needed.`,
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

    const commitTypes = new Set(parsedCommits.map((commit) => commit.type).filter(Boolean) as string[]);
    const refactorSignal =
        commitTypes.has("refactor") ||
        /refactor/i.test(input.prTitle || "") ||
        input.labels.some((label) => /refactor/i.test(label));

    const cleanedBody = (input.prBody || "").trim();
    if (cleanedBody.length < 40) {
        // === PR description completeness warnings ===
        warnings.push("PR description looks empty. Please add a short rationale and testing notes.");
    } else if (
        scopes.totals.source.files > 0 &&
        scopes.totals["golden-tests"].files === 0 &&
        !/test(ed|ing)?/i.test(cleanedBody)
    ) {
        // === Missing testing notes warnings (only when source changed and golden tests did not) ===
        warnings.push("Add a brief note about how this change was tested (or why tests are not needed).");
    }

    const skipTests = hasSkipTests(input.prBody || "", input.labels);
    if (
        !skipTests &&
        !refactorSignal &&
        scopes.totals.source.files > 0 &&
        scopes.totals.tests.files === 0 &&
        scopes.totals["golden-tests"].files === 0
    ) {
        // === Source changes without tests/fixtures warnings ===
        warnings.push("Source changed but no tests or fixtures were updated.");
    }

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

    const infos = commitSizeInfos(input.commits);

    const warnings = [
        ...commitValidation.warnings,
        ...basicChecks(input, summary, commitValidation.parsed),
    ];

    return { warnings, infos, summary };
}
