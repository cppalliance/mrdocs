//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef INFO
#define INFO(PascalName)
#endif

// Custom flag commands: block-command syntax that sets a boolean flag
// on the symbol instead of producing a documentation node. The command
// spelling is the lowercased PascalName (e.g. `@functionobject`); the
// matching DocComment field is `Is<PascalName>`. Aliases for the same
// flag are not listed here; they are handled where the flag is.
INFO(FunctionObject)
INFO(SeeBelow)
INFO(ImplementationDefined)

#undef INFO
