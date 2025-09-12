//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_IO_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_IO_H

int
_access(char *path, int mode);

int
_chmod(const char *pathname, int pmode);

int
_chsize(int handle, long size);

int
_close(int handle);

int
_commit(int handle);

int
_creat(const char *name, int pmode);

int
_dup(int fd);

int
dup2(int fdl, int fd2);

int
_eof(int handle);

long
_filelength(int fd);

int
_isatty(int fd);

int
_locking(int fd, int mode, long size);

long
_lseek(int fd, long offset, int mode);

char *
_mktemp(char *template_);

int
_open(const char *file, int oflag, int pmode);

int
_open(const char *file, int oflag);

int
_read(int fd, void *buffer, unsigned int len);

int
_setmode(int handle, int mode);

int
_sopen(const char *file, int oflag, int shflag, int pmode);

int
_sopen(const char *file, int oflag, int shflag);

long
_tell(int handle);

int
_write(int fd, void *buffer, unsigned int length);

unsigned
_umask(int mode);

int
_unlink(const char *filename);

long
filesize(const char *filename);

unsigned short
getDS(void);

int
getftime(int handle, struct ftime *ftimep);

int
lock(int handle, long offset, long length);

int
remove(const char *filename);

int
rename(const char *oldname, const char *newname);

int
setftime(int handle, struct ftime *ftime);

int
unlock(int handle, long offset, long length);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_IO_H
