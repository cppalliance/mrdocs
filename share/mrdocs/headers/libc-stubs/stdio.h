//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDIO_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDIO_H

#include "stddef.h"
#include "stdarg.h"

typedef long fpos_t;

#define stdin  ((FILE *)0)
#define stdout ((FILE *)1)
#define stderr ((FILE *)2)

FILE* fopen(const char* filename, const char* mode);
FILE* fopen_s(FILE** file, const char* filename, const char* mode);
FILE* freopen(const char* filename, const char* mode, FILE* stream);
FILE* freopen_s(FILE** file, const char* filename, const char* mode, FILE* stream);
int fclose(FILE* stream);
int fflush(FILE* stream);
void setbuf(FILE* stream, char* buffer);
int setvbuf(FILE* stream, char* buffer, int mode, size_t size);
int fwide(FILE* stream, int mode);

size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);

int fgetc(FILE* stream);
int getc(FILE* stream);
char* fgets(char* str, int n, FILE* stream);
int fputc(int c, FILE* stream);
int putc(int c, FILE* stream);
int fputs(const char* str, FILE* stream);
int getchar(void);
char* gets(char* str);
int gets_s(char* str, rsize_t n);
int putchar(int c);
int puts(const char* str);
int ungetc(int c, FILE* stream);

wint_t fgetwc(FILE* stream);
wint_t getwc(FILE* stream);
wchar_t* fgetws(wchar_t* str, int n, FILE* stream);
wint_t fputwc(wint_t wc, FILE* stream);
wint_t putwc(wint_t wc, FILE* stream);
int fputws(const wchar_t* str, FILE* stream);
wint_t getwchar(void);
wint_t putwchar(wint_t wc);
wint_t ungetwc(wint_t wc, FILE* stream);

int scanf(const char* format, ...);
int fscanf(FILE* stream, const char* format, ...);
int sscanf(const char* str, const char* format, ...);
int scanf_s(const char* format, ...);
int fscanf_s(FILE* stream, const char* format, ...);
int sscanf_s(const char* str, const char* format, ...);
int vscanf(const char* format, va_list arg);
int vfscanf(FILE* stream, const char* format, va_list arg);
int vsscanf(const char* str, const char* format, va_list arg);
int vscanf_s(const char* format, va_list arg);
int vfscanf_s(FILE* stream, const char* format, va_list arg);
int vsscanf_s(const char* str, const char* format, va_list arg);

int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int printf_s(const char* format, ...);
int fprintf_s(FILE* stream, const char* format, ...);
int sprintf_s(char* str, size_t size, const char* format, ...);
int snprintf_s(char* str, size_t size, const char* format, ...);
int vprintf(const char* format, va_list arg);
int vfprintf(FILE* stream, const char* format, va_list arg);
int vsprintf(char* str, const char* format, va_list arg);
int vsnprintf(char* str, size_t size, const char* format, va_list arg);
int vprintf_s(const char* format, va_list arg);
int vfprintf_s(FILE* stream, const char* format, va_list arg);
int vsprintf_s(char* str, size_t size, const char* format, va_list arg);
int vsnprintf_s(char* str, size_t size, const char* format, va_list arg);

int wscanf(const wchar_t* format, ...);
int fwscanf(FILE* stream, const wchar_t* format, ...);
int swscanf(const wchar_t* str, const wchar_t* format, ...);
int wscanf_s(const wchar_t* format, ...);
int fwscanf_s(FILE* stream, const wchar_t* format, ...);
int swscanf_s(const wchar_t* str, const wchar_t* format, ...);
int vwscanf(const wchar_t* format, va_list arg);
int vfwscanf(FILE* stream, const wchar_t* format, va_list arg);
int vswscanf(const wchar_t* str, const wchar_t* format, va_list arg);
int vwscanf_s(const wchar_t* format, va_list arg);
int vfwscanf_s(FILE* stream, const wchar_t* format, va_list arg);
int vswscanf_s(const wchar_t* str, const wchar_t* format, va_list arg);

int wprintf(const wchar_t* format, ...);
int fwprintf(FILE* stream, const wchar_t* format, ...);
int swprintf(wchar_t* str, const wchar_t* format, ...);
int wprintf_s(const wchar_t* format, ...);
int fwprintf_s(FILE* stream, const wchar_t* format, ...);
int swprintf_s(wchar_t* str, size_t size, const wchar_t* format, ...);
int snwprintf_s(wchar_t* str, size_t size, const wchar_t* format, ...);
int vwprintf(const wchar_t* format, va_list arg);
int vfwprintf(FILE* stream, const wchar_t* format, va_list arg);
int vswprintf(wchar_t* str, const wchar_t* format, va_list arg);
int vwprintf_s(const wchar_t* format, va_list arg);
int vfwprintf_s(FILE* stream, const wchar_t* format, va_list arg);
int vswprintf_s(wchar_t* str, size_t size, const wchar_t* format, va_list arg);
int vsnwprintf_s(wchar_t* str, size_t size, const wchar_t* format, va_list arg);

long ftell(FILE* stream);
int fgetpos(FILE* stream, fpos_t* pos);
int fseek(FILE* stream, long offset, int whence);
int fsetpos(FILE* stream, const fpos_t* pos);
void rewind(FILE* stream);

void clearerr(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void perror(const char* str);

int remove(const char* filename);
int rename(const char* old_filename, const char* new_filename);
FILE* tmpfile(void);
FILE* tmpfile_s(FILE** file);
char* tmpnam(char* str);
char* tmpnam_s(char* str, rsize_t size);

#define EOF (-1)
#define FOPEN_MAX 20
#define FILENAME_MAX 260
#define BUFSIZ 512
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define TMP_MAX 238328
#define TMP_MAX_S 238328
#define L_tmpnam 20
#define L_tmpnam_s 20

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDIO_H
