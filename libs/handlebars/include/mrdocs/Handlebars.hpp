//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HPP
#define MRDOCS_API_HANDLEBARS_HPP

// Umbrella header for the Handlebars engine. Each class and helper category has
// its own header under mrdocs/Handlebars/. The names live in the
// mrdocs::handlebars namespace; callers spell that out explicitly, or alias it
// locally where convenient (e.g. `namespace hbs = mrdocs::handlebars;`).

#include <mrdocs/Handlebars/Error.hpp>
#include <mrdocs/Handlebars/Platform.hpp>
#include <mrdocs/Handlebars/OutputRef.hpp>
#include <mrdocs/Handlebars/Options.hpp>
#include <mrdocs/Handlebars/Engine.hpp>
#include <mrdocs/Handlebars/Helpers/Builtin.hpp>
#include <mrdocs/Handlebars/Helpers/Constructor.hpp>
#include <mrdocs/Handlebars/Helpers/Logical.hpp>
#include <mrdocs/Handlebars/Helpers/Math.hpp>
#include <mrdocs/Handlebars/Helpers/String.hpp>
#include <mrdocs/Handlebars/Helpers/Container.hpp>

#endif
