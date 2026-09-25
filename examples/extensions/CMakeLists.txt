#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Corpus-transform extension examples. Each runs in its own directory, so a
# transform can resolve the external files it reads (for example
# ghostwriter's external-docs/) relative to the run.

# ghostwriter commits the reference it produces, which the Corpus Transforms
# page includes.
mrdocs_add_example(DIRECTORY ghostwriter CONFIG mrdocs.yml
    OUTPUTS reference.adoc)

# project-conventions commits no reference; its test checks that the run
# succeeds. What the page shows is the extension's warnings: report.py
# captures them from a run as warnings.txt, which is compared instead.
mrdocs_add_example(DIRECTORY project-conventions CONFIG mrdocs.yml RUN_ONLY)
mrdocs_add_example(DIRECTORY project-conventions
    NAME extensions-project-conventions-report
    SCRIPT report.py OUTPUTS warnings.txt)
