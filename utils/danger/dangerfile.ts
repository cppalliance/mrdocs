//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//
import { runDanger } from "./runner";

// Provided globally by Danger at runtime; declared here for editors/typecheckers.
declare function markdown(message: string, file?: string, line?: number): void;

/**
 * Entrypoint for Danger; delegates to the rule runner.
 * Wraps execution to surface unexpected errors as warnings instead of failing CI.
 */
export default async function dangerfile(): Promise<void> {
    try {
        await runDanger();
    } catch (error) {
        markdown(["> [!WARNING]", `> Danger checks hit an unexpected error: ${String(error)}`].join("\n"));
    }
}
