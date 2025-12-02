//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//
import type { DangerDSLType } from "danger";
import { evaluateDanger, type CommitInfo, type FileChange } from "./logic";

// Danger provides these as globals at runtime; we declare them for editors/typecheckers.
declare const danger: DangerDSLType;
declare function warn(message: string, file?: string, line?: number): void;
declare function markdown(message: string, file?: string, line?: number): void;

/**
 * Retrieve the list of changed files with basic stats for scope summaries.
 *
 * @returns files touched in the PR including add/delete counts.
 */
async function fetchChangedFiles(): Promise<FileChange[]> {
    const api = danger.github.api;
    const pr = danger.github.pr;
    const files: FileChange[] = [];
    let page = 1;

    while (true) {
        const response = await api.pulls.listFiles({
            owner: pr.base.repo.owner.login,
            repo: pr.base.repo.name,
            pull_number: pr.number,
            per_page: 100,
            page,
        });

        for (const file of response.data) {
            files.push({
                filename: file.filename,
                additions: file.additions ?? 0,
                deletions: file.deletions ?? 0,
                status: file.status,
            });
        }

        if (response.data.length < 100) {
            break;
        }
        page += 1;
    }

    return files;
}

/**
 * Pull commit messages and per-commit file stats so size checks can ignore test churn.
 *
 * @returns commits enriched with file-level additions/deletions.
 */
async function fetchCommitDetails(): Promise<CommitInfo[]> {
    const api = danger.github.api;
    const commits = danger.github.commits;
    const pr = danger.github.pr;
    const owner = pr.base.repo.owner.login;
    const repo = pr.base.repo.name;

    const enriched: CommitInfo[] = [];
    for (const commit of commits) {
        let files: FileChange[] | undefined;
        try {
            const response = await api.repos.getCommit({
                owner,
                repo,
                ref: commit.sha,
            });
            files = (response.data.files || []).map((file) => ({
                filename: file.filename,
                additions: file.additions ?? 0,
                deletions: file.deletions ?? 0,
                status: file.status,
            }));
        } catch (error) {
            warn(`Unable to load file stats for commit ${commit.sha}: ${String(error)}`);
        }

        enriched.push({
            sha: commit.sha,
            message: commit.commit.message,
            files,
        });
    }
    return enriched;
}

/**
 * Main runner: gathers PR context and feeds it into rule evaluation.
 */
export async function runDanger(): Promise<void> {
    if (!danger.github || !danger.github.pr) {
        markdown("Danger checks are only available on pull requests.");
        return;
    }

    const [files, commits] = await Promise.all([fetchChangedFiles(), fetchCommitDetails()]);
    const pr = danger.github.pr;
    const labels = (danger.github.issue?.labels || []).map((label) => label.name);

    const result = evaluateDanger({
        files,
        commits,
        prBody: pr.body || "",
        prTitle: pr.title || "",
        labels,
    });

    const experimentalNote =
        "\n_Note: Danger.js checks for MrDocs are experimental; some warnings may still be noisy or false positives._";

    markdown(
        [
            result.summary.markdown,
            result.summary.highlights.length > 0 ? "\n**Highlights:**\n- " + result.summary.highlights.join("\n- ") : "",
            experimentalNote,
        ]
            .filter(Boolean)
            .join("\n"),
    );

    for (const message of result.warnings) {
        warn(message);
    }
}
