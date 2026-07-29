//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_CTYPE_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_CTYPE_H

// Character classification
int isalnum( int ch );
int isalpha( int ch );
int islower( int ch );
int isupper( int ch );
int isdigit( int ch );
int isxdigit( int ch );
int iscntrl( int ch );
int isgraph( int ch );
int isspace( int ch );
int isblank( int ch );
int isprint( int ch );
int ispunct( int ch );

// Character manipulation
int tolower( int ch );
int toupper( int ch );

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_CTYPE_H
