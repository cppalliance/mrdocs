#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

echo "Building documentation with Antora..."
echo "Installing npm dependencies..."
npm ci

echo "Building docs in custom dir..."
npx antora --clean --fetch antora-playbook.yml --attribute branchesarray=HEAD
echo "Done"
