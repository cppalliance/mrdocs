//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//
import { formatChurn, scopeDisplayOrder, type DangerResult, type ScopeReport, type ScopeTotals } from "./logic";

const notice = "> 🚧 Danger.js checks for MrDocs are experimental; expect some rough edges while we tune the rules.";

const scopeLabels: Record<string, string> = {
    source: "Source",
    tests: "Tests",
    "golden-tests": "Golden Tests",
    docs: "Docs",
    ci: "CI / Roadmap",
    build: "Build / Toolchain",
    tooling: "Tooling",
    "third-party": "Third-party",
    other: "Other",
};

function labelForScope(scope: string): string {
    return scopeLabels[scope] ?? scope;
}

/**
 * Pad cells so pipes align in the raw Markdown.
 */
function renderAlignedTable(headers: string[], alignRight: boolean[], rows: string[][]): string {
    const allRows = [headers, ...rows];
    const widths = headers.map((_, col) => Math.max(...allRows.map((r) => r[col].length)));

    const formatCell = (text: string, col: number, right: boolean): string => {
        const width = widths[col];
        return right ? text.padStart(width, " ") : text.padEnd(width, " ");
    };

    const renderRow = (cells: string[], isHeader = false): string =>
        `| ${cells
            .map((cell, idx) => formatCell(cell, idx, alignRight[idx]))
            .join(" | ")} |`;

    const headerRow = renderRow(headers, true);
    const separatorCells = headers.map((_, idx) => {
        const width = Math.max(3, widths[idx]);
        if (alignRight[idx]) {
            const dashes = Math.max(3, width) - 1;
            return "-".repeat(dashes) + ":";
        }
        return "-".repeat(width);
    });
    const separatorRow = `| ${separatorCells.join(" | ")} |`;
    const bodyRows = rows.map((r) => renderRow(r));

    return [headerRow, separatorRow, ...bodyRows].join("\n");
}

/**
 * Format counts so zeros stay quiet while non-zero values draw attention.
 */
function formatCount(value: number, bold: boolean = true): string {
    if (value === 0) {
        return "-";
    }
    return bold ? `**${value}**` : `${value}`;
}

/**
 * Render warnings as GitHub admonition blocks grouped under a heading.
 */
function renderWarnings(warnings: string[]): string {
    if (warnings.length === 0) {
        return "";
    }
    const blocks = warnings.map((message) => ["> [!WARNING]", `> ${message}`].join("\n"));
    return ["## ⚠️ Warnings", blocks.join("\n\n")].join("\n");
}

function renderInfos(infos: string[]): string {
    if (infos.length === 0) {
        return "";
    }
    const blocks = infos.map((message) => ["> [!NOTE]", `> ${message}`].join("\n"));
    return ["## ℹ️ Info", blocks.join("\n\n")].join("\n");
}

function countFileChanges(status: ScopeTotals["status"]): number {
    return status.added + status.modified + status.renamed + status.removed + status.other;
}

/**
 * Render a single table combining change summary and per-scope breakdown.
 */
function renderChangeTable(summary: ScopeReport): string {
    const headers = ["Scope", "Lines Δ", "Lines +", "Lines -", "Files Δ", "Files +", "Files ~", "Files ↔", "Files -"];
    const alignRight = [false, true, true, true, true, true, true, true, true];

    const sortedScopes = [...scopeDisplayOrder].sort((a, b) => {
        const ta = summary.totals[a];
        const tb = summary.totals[b];
        const churnA = ta.additions + ta.deletions;
        const churnB = tb.additions + tb.deletions;
        if (churnA === churnB) {
            return tb.files - ta.files;
        }
        return churnB - churnA;
    });

    const scopeHasChange = (totals: ScopeTotals): boolean => {
        const churn = totals.additions + totals.deletions;
        const fileDelta = countFileChanges(totals.status);
        return churn !== 0 || fileDelta !== 0;
    };

    const scopeRows = sortedScopes.filter((scope) => scopeHasChange(summary.totals[scope])).map((scope) => {
        const scoped: ScopeTotals = summary.totals[scope];
        const s = scoped.status;
        const fileDelta = countFileChanges(s);
        const churn = scoped.additions + scoped.deletions;
        const fileDeltaBold = formatCount(fileDelta); // bold delta
        const label = labelForScope(scope);
        return [
            label,
            formatCount(churn),
            formatCount(scoped.additions, false),
            formatCount(scoped.deletions, false),
            fileDeltaBold,
            formatCount(s.added, false),
            formatCount(s.modified, false),
            formatCount(s.renamed, false),
            formatCount(s.removed, false),
        ];
    });

    const total = summary.overall;
    const totalStatus = total.status;
    const totalChurn = total.additions + total.deletions;
    const totalFileDelta = countFileChanges(totalStatus);
    const totalRow = [
        "**Total**",
        formatCount(totalChurn),
        formatCount(total.additions, false),
        formatCount(total.deletions, false),
        formatCount(totalFileDelta),
        formatCount(totalStatus.added, false),
        formatCount(totalStatus.modified, false),
        formatCount(totalStatus.renamed, false),
        formatCount(totalStatus.removed, false),
    ];

    const rows = scopeRows.length > 0 ? scopeRows.concat([totalRow]) : [["(no changes)", "-", "-", "-", "-", "-", "-", "-", "-"]];

    const table = renderAlignedTable(headers, alignRight, rows);
    const legend = "Legend: Files + (added), Files ~ (modified), Files ↔ (renamed), Files - (removed)";

    return ["## 🧾 Changes by Scope", table, "", `> ${legend}`].join("\n");
}

/**
 * Render highlight bullets, keeping the section compact.
 */
function renderHighlights(highlights: string[]): string {
    if (highlights.length === 0) {
        return "## ✨ Highlights\n- None noted.";
    }
    const decorated = highlights.map((note) => {
        const lower = note.toLowerCase();
        if (lower.includes("golden")) {
            return `- 🧪 ${note}`;
        }
        if (lower.includes("test")) {
            return `- 🧪 ${note}`;
        }
        if (lower.includes("doc")) {
            return `- 📄 ${note}`;
        }
        if (lower.includes("source")) {
            return `- 🛠️ ${note}`;
        }
        if (lower.includes("ci") || lower.includes("workflow") || lower.includes("pipeline")) {
            return `- ⚙️ ${note}`;
        }
        if (lower.includes("build")) {
            return `- 🏗️ ${note}`;
        }
        return `- ✨ ${note}`;
    });
    return ["## ✨ Highlights", ...decorated].join("\n");
}

/**
 * Render a short "Top changes" summary from the highest-churn scopes.
 */
function renderTopChanges(summary: ScopeReport): string {
    if (!summary.topFiles || summary.topFiles.length === 0) {
        return "";
    }

    const bullets = summary.topFiles.map((file) => {
        const scopeLabel = labelForScope(file.scope);
        return `- ${file.filename} (${scopeLabel}): **${file.churn}** lines Δ (+${file.additions} / -${file.deletions})`;
    });

    return ["### 🔝 Top Files", ...bullets].join("\n");
}

/**
 * Build the full Danger Markdown report as a single, structured comment.
 */
export function renderDangerReport(result: DangerResult): string {
    const sections = [
        notice,
        renderWarnings(result.warnings),
        renderInfos(result.infos),
        renderHighlights(result.summary.highlights),
        renderChangeTable(result.summary),
        renderTopChanges(result.summary),
    ].filter(Boolean);

    return sections.join("\n\n");
}
