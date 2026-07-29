//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_UNISTD_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_UNISTD_H

#include "stddef.h"
#include "stdint.h"
#include "sys/types.h"

// access()
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

// confstr() constants
#define _CS_PATH 1
#define _CS_XBS5_ILP32_OFF32_CFLAGS 2
#define _CS_XBS5_ILP32_OFF32_FLAGS 3
#define _CS_XBS5_ILP32_OFF32_LIB 4
#define _CS_XBS5_ILP32_OFF32_LINTFLAGS 5
#define _CS_XBS5_ILP32_OFFBIG_CFLAGS 6
#define _CS_XBS5_ILP32_OFFBIG_LDFLAGS 7
#define _CS_XBS5_ILP32_OFFBIG_LIBS 8
#define _CS_XBS5_ILP32_OFFBIG_LINTFLAGS 9
#define _CS_XBS5_LP64_OFF64_CFLAGS 10
#define _CS_XBS5_LP64_OFF64_FLAGS 11
#define _CS_XBS5_LP64_OFF64_LIB 12
#define _CS_XBS5_LP64_OFF64_LINTFLAGS 13
#define _CS_XBS5_LPBIG_OFFBIG_CFLAGS 14
#define _CS_XBS5_LPBIG_OFFBIG_LDFLAGS 15
#define _CS_XBS5_LPBIG_OFFBIG_LIBS 16
#define _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS 17

// lseek() constants
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// sysconf()
#define _SC_2_C_BIND 1
#define _SC_2_C_DEV 2
#define _SC_2_C_VERSION 3
#define _SC_2_FORT_DEV 4
#define _SC_2_FORT_RUN 5
#define _SC_2_LOCALEDEF 6
#define _SC_2_SW_DEV 7
#define _SC_2_UPE 8
#define _SC_2_VERSION 9
#define _SC_ARG_MAX 10
#define _SC_AIO_LISTIO_MAX 11
#define _SC_AIO_MAX 12
#define _SC_AIO_PRIO_DELTA_MAX 13
#define _SC_ASYNCHRONOUS_IO 14
#define _SC_ATEXIT_MAX 15
#define _SC_BC_BASE_MAX 16
#define _SC_BC_DIM_MAX 17
#define _SC_BC_SCALE_MAX 18
#define _SC_BC_STRING_MAX 19
#define _SC_CHILD_MAX 20
#define _SC_CLK_TCK 21
#define _SC_COLL_WEIGHTS_MAX 22
#define _SC_DELAYTIMER_MAX 23
#define _SC_EXPR_NEST_MAX 24
#define _SC_FSYNC 25
#define _SC_GETGR_R_SIZE_MAX 26
#define _SC_GETPW_R_SIZE_MAX 27
#define _SC_IOV_MAX 28
#define _SC_JOB_CONTROL 29
#define _SC_LINE_MAX 30
#define _SC_LOGIN_NAME_MAX 31
#define _SC_MAPPED_FILES 32
#define _SC_MEMLOCK 33
#define _SC_MEMLOCK_RANGE 34
#define _SC_MEMORY_PROTECTION 35
#define _SC_MESSAGE_PASSING 36
#define _SC_MQ_OPEN_MAX 37
#define _SC_MQ_PRIO_MAX 38
#define _SC_NGROUPS_MAX 39
#define _SC_OPEN_MAX 40
#define _SC_PAGESIZE 41
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_PASS_MAX 42
#define _SC_PRIORITIZED_IO 43
#define _SC_PRIORITY_SCHEDULING 44
#define _SC_RE_DUP_MAX 45
#define _SC_REALTIME_SIGNALS 46
#define _SC_RTSIG_MAX 47
#define _SC_SAVED_IDS 48
#define _SC_SEMAPHORES 49
#define _SC_SEM_NSEMS_MAX 50
#define _SC_SEM_VALUE_MAX 51
#define _SC_SHARED_MEMORY_OBJECTS 52
#define _SC_SIGQUEUE_MAX 53
#define _SC_STREAM_MAX 54
#define _SC_SYNCHRONIZED_IO 55
#define _SC_THREADS 56
#define _SC_THREAD_ATTR_STACKADDR 57
#define _SC_THREAD_ATTR_STACKSIZE 58
#define _SC_THREAD_DESTRUCTOR_ITERATIONS 59
#define _SC_THREAD_KEYS_MAX 60
#define _SC_THREAD_PRIORITY_SCHEDULING 61
#define _SC_THREAD_PRIO_INHERIT 62
#define _SC_THREAD_PRIO_PROTECT 63
#define _SC_THREAD_PROCESS_SHARED 64
#define _SC_THREAD_SAFE_FUNCTIONS 65
#define _SC_THREAD_STACK_MIN 66
#define _SC_THREAD_THREADS_MAX 67
#define _SC_TIMERS 68
#define _SC_TIMER_MAX 69
#define _SC_TTY_NAME_MAX 70
#define _SC_TZNAME_MAX 71
#define _SC_VERSION 72
#define _SC_XOPEN_VERSION 73
#define _SC_XOPEN_CRYPT 74
#define _SC_XOPEN_ENH_I18N 75
#define _SC_XOPEN_SHM 76
#define _SC_XOPEN_UNIX 77
#define _SC_XOPEN_XCU_VERSION 78
#define _SC_XOPEN_LEGACY 79
#define _SC_XOPEN_REALTIME 80
#define _SC_XOPEN_REALTIME_THREADS 81
#define _SC_XBS5_ILP32_OFF32 82
#define _SC_XBS5_ILP32_OFF 83
#define _SC_XBS5_LP64_OFF64 84
#define _SC_XBS5_LPBIG_OFFBIG 85

// lockf() constants
#define F_LOCK 1
#define F_ULOCK 2
#define F_TEST 3
#define F_BLOCK 4

// pathconf() and fpathconf() constants
#define _PC_ASYNC_IO 1
#define _PC_CHOWN_RESTRICTED 2
#define _PC_FILESIZEBITS 3
#define _PC_LINK_MAX 4
#define _PC_MAX_CANON 5
#define _PC_MAX_INPUT 6
#define _PC_NAME_MAX 7
#define _PC_NO_TRUNC 8
#define _PC_PATH_MAX 9
#define _PC_PIPE_BUF 10
#define _PC_PRIO_IO 11
#define _PC_SYNC_IO 12
#define _PC_VDISABLE 13

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int          access(const char *, int);
unsigned int alarm(unsigned int);
int          brk(void *);
int          chdir(const char *);
int          chroot(const char *);
int          chown(const char *, uid_t, gid_t);
int          close(int);
size_t       confstr(int, char *, size_t);
char        *crypt(const char *, const char *);
char        *ctermid(char *);
char        *cuserid(char *s);
int          dup(int);
int          dup2(int, int);
void         encrypt(char[64], int);
int          execl(const char *, const char *, ...);
int          execle(const char *, const char *, ...);
int          execlp(const char *, const char *, ...);
int          execv(const char *, char *const []);
int          execve(const char *, char *const [], char *const []);
int          execvp(const char *, char *const []);
void        _exit(int);
int          fchown(int, uid_t, gid_t);
int          fchdir(int);
int          fdatasync(int);
pid_t        fork(void);
long int     fpathconf(int, int);
int          fsync(int);
int          ftruncate(int, off_t);
char        *getcwd(char *, size_t);
int          getdtablesize(void);
gid_t        getegid(void);
uid_t        geteuid(void);
gid_t        getgid(void);
int          getgroups(int, gid_t []);
long         gethostid(void);
char        *getlogin(void);
int          getlogin_r(char *, size_t);
int          getopt(int, char * const [], const char *);
int          getpagesize(void);
char        *getpass(const char *);
pid_t        getpgid(pid_t);
pid_t        getpgrp(void);
pid_t        getpid(void);
pid_t        getppid(void);
pid_t        getsid(pid_t);
uid_t        getuid(void);
char        *getwd(char *);
int          isatty(int);
int          lchown(const char *, uid_t, gid_t);
int          link(const char *, const char *);
int          lockf(int, int, off_t);
off_t        lseek(int, off_t, int);
int          nice(int);
long int     pathconf(const char *, int);
int          pause(void);
int          pipe(int [2]);
ssize_t      pread(int, void *, size_t, off_t);
int          pthread_atfork(void (*)(void), void (*)(void),
                 void(*)(void));
ssize_t      pwrite(int, const void *, size_t, off_t);
ssize_t      read(int, void *, size_t);
int          readlink(const char *, char *, size_t);
int          rmdir(const char *);
void        *sbrk(intptr_t);
int          setgid(gid_t);
int          setpgid(pid_t, pid_t);
pid_t        setpgrp(void);
int          setregid(gid_t, gid_t);
int          setreuid(uid_t, uid_t);
pid_t        setsid(void);
int          setuid(uid_t);
unsigned int sleep(unsigned int);
void         swab(const void *, void *, ssize_t);
int          symlink(const char *, const char *);
void         sync(void);
long int     sysconf(int);
pid_t        tcgetpgrp(int);
int          tcsetpgrp(int, pid_t);
int          truncate(const char *, off_t);
char        *ttyname(int);
int          ttyname_r(int, char *, size_t);
useconds_t   ualarm(useconds_t, useconds_t);
int          unlink(const char *);
int          usleep(useconds_t);
pid_t        vfork(void);
ssize_t      write(int, const void *, size_t);

extern char   *optarg;
extern int    optind, opterr, optopt;

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_UNISTD_H
