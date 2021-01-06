void initialise_monitor_handles(void);  // XXX what is this?

// Mostly snarfed from /usr/include/newlib/sys/*.h
typedef int pid_t;
int     _close (int __fildes);
pid_t   _fork (void);
pid_t   _getpid (void);
int     _isatty (int __fildes);
int     _link (const char *__path1, const char *__path2);
void *  _sbrk (ptrdiff_t __incr);
int     _unlink (const char *__path);
int     _kill (pid_t, int);
int     _read (int __fd, void *__buf, size_t __nbyte);
int     _write (int __fd, const void *__buf, size_t __nbyte);
clock_t _times (struct tms *);
int     _execve (const char *__path, char * const __argv[], char * const __envp[]);
int     _stat (const char *__restrict __path, struct stat *__restrict __sbuf);
int     _fstat (int __fd, struct stat *__sbuf );
off_t   _lseek (int __fildes, _off_t __offset, int __whence);
pid_t   _wait (int *);
int     _open (const char *, int, ...);
