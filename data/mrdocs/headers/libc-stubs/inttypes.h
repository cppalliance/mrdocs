//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_INTTYPES_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_INTTYPES_H

#include "stdint.h"

// Format constants for the fprintf family of functions
#define PRId8 "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 "lld"

#define PRIi8 "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 "lli"

#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "llu"

#define PRIo8 "o"
#define PRIo16 "o"
#define PRIo32 "o"
#define PRIo64 "llo"

#define PRIx8 "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 "llx"

#define PRIX8 "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 "llX"

#define PRIdLEAST8 "d"
#define PRIdLEAST16 "d"
#define PRIdLEAST32 "d"
#define PRIdLEAST64 "lld"

#define PRIiLEAST8 "i"
#define PRIiLEAST16 "i"
#define PRIiLEAST32 "i"
#define PRIiLEAST64 "lli"

#define PRIuLEAST8 "u"
#define PRIuLEAST16 "u"
#define PRIuLEAST32 "u"
#define PRIuLEAST64 "llu"

#define PRIoLEAST8 "o"
#define PRIoLEAST16 "o"
#define PRIoLEAST32 "o"
#define PRIoLEAST64 "llo"

#define PRIxLEAST8 "x"
#define PRIxLEAST16 "x"
#define PRIxLEAST32 "x"
#define PRIxLEAST64 "llx"

#define PRIXLEAST8 "X"
#define PRIXLEAST16 "X"
#define PRIXLEAST32 "X"
#define PRIXLEAST64 "llX"

#define PRIdFAST8 "d"
#define PRIdFAST16 "d"
#define PRIdFAST32 "d"
#define PRIdFAST64 "lld"

#define PRIiFAST8 "i"
#define PRIiFAST16 "i"
#define PRIiFAST32 "i"
#define PRIiFAST64 "lli"

#define PRIuFAST8 "u"
#define PRIuFAST16 "u"
#define PRIuFAST32 "u"
#define PRIuFAST64 "llu"

#define PRIoFAST8 "o"
#define PRIoFAST16 "o"
#define PRIoFAST32 "o"
#define PRIoFAST64 "llo"

#define PRIxFAST8 "x"
#define PRIxFAST16 "x"
#define PRIxFAST32 "x"
#define PRIxFAST64 "llx"

#define PRIXFAST8 "X"
#define PRIXFAST16 "X"
#define PRIXFAST32 "X"
#define PRIXFAST64 "llX"

#define PRIdMAX "jd"
#define PRIiMAX "ji"
#define PRIuMAX "ju"
#define PRIoMAX "jo"
#define PRIxMAX "jx"
#define PRIXMAX "jX"

#define PRIdPTR "td"
#define PRIiPTR "ti"
#define PRIuPTR "tu"
#define PRIoPTR "to"
#define PRIxPTR "tx"
#define PRIXPTR "tX"

// Format constants for the fscanf family of functions
#define SCNd8 "hhd"
#define SCNd16 "hd"
#define SCNd32 "d"
#define SCNd64 "lld"

#define SCNi8 "hhi"
#define SCNi16 "hi"
#define SCNi32 "i"
#define SCNi64 "lli"

#define SCNu8 "hhu"
#define SCNu16 "hu"
#define SCNu32 "u"
#define SCNu64 "llu"

#define SCNo8 "hho"
#define SCNo16 "ho"
#define SCNo32 "o"
#define SCNo64 "llo"

#define SCNx8 "hhx"
#define SCNx16 "hx"
#define SCNx32 "x"
#define SCNx64 "llx"

#define SCNdLEAST8 "hhd"
#define SCNdLEAST16 "hd"
#define SCNdLEAST32 "d"
#define SCNdLEAST64 "lld"

#define SCNiLEAST8 "hhi"
#define SCNiLEAST16 "hi"
#define SCNiLEAST32 "i"
#define SCNiLEAST64 "lli"

#define SCNuLEAST8 "hhu"
#define SCNuLEAST16 "hu"
#define SCNuLEAST32 "u"
#define SCNuLEAST64 "llu"

#define SCNoLEAST8 "hho"
#define SCNoLEAST16 "ho"
#define SCNoLEAST32 "o"
#define SCNoLEAST64 "llo"

#define SCNxLEAST8 "hhx"
#define SCNxLEAST16 "hx"
#define SCNxLEAST32 "x"
#define SCNxLEAST64 "llx"

#define SCNdFAST8 "hhd"
#define SCNdFAST16 "hd"
#define SCNdFAST32 "d"
#define SCNdFAST64 "lld"

#define SCNiFAST8 "hhi"
#define SCNiFAST16 "hi"
#define SCNiFAST32 "i"
#define SCNiFAST64 "lli"

#define SCNuFAST8 "hhu"
#define SCNuFAST16 "hu"
#define SCNuFAST32 "u"
#define SCNuFAST64 "llu"

#define SCNoFAST8 "hho"
#define SCNoFAST16 "ho"
#define SCNoFAST32 "o"
#define SCNoFAST64 "llo"

#define SCNxFAST8 "hhx"
#define SCNxFAST16 "hx"
#define SCNxFAST32 "x"
#define SCNxFAST64 "llx"

#define SCNdMAX "jd"
#define SCNiMAX "ji"
#define SCNuMAX "ju"
#define SCNoMAX "jo"
#define SCNxMAX "jx"

#define SCNdPTR "td"
#define SCNiPTR "ti"
#define SCNuPTR "tu"
#define SCNoPTR "to"
#define SCNxPTR "tx"

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_INTTYPES_H
