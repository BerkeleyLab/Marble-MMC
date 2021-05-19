/**
*****************************************************************************
**
**  File        : syscalls.c
**
**  Abstract    : System Workbench Minimal System calls file
**
**                For more information about which c-functions
**                need which of these lowlevel functions
**                please consult the Newlib libc-manual
**
**  Environment : System Workbench for MCU
**
**  Distribution: The file is distributed "as is," without any warranty
**                of any kind.
**
*****************************************************************************
**
** <h2><center>&copy; COPYRIGHT(c) 2014 Ac6</center></h2>
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**   1. Redistributions of source code must retain the above copyright notice,
**      this list of conditions and the following disclaimer.
**   2. Redistributions in binary form must reproduce the above copyright notice,
**      this list of conditions and the following disclaimer in the documentation
**      and/or other materials provided with the distribution.
**   3. Neither the name of Ac6 nor the names of its contributors
**      may be used to endorse or promote products derived from this software
**      without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************************
*/

/* Includes */
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include "fake_newlib.h"


/* Variables */
#undef errno
extern int errno;
extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

char *__env[1] = { 0 };
char **environ = __env;


/* Functions */
void initialise_monitor_handles(void)
{
}

int _getpid(void)
{
	return 1;
}

int _kill(int pid, int sig)
{
	(void) pid; (void) sig;
	errno = EINVAL;
	return -1;
}

#if 0
void _exit (int status)
{
	_kill(status, -1);
	while (1) {}  /* Make sure we hang here */
}
#endif

__attribute__((weak)) int _read(int file, void *ptr, size_t len)
{
	(void) file;  // no files here
	char *p = ptr;
	for (size_t DataIdx = 0; DataIdx < len; DataIdx++)
	{
		*p++ = __io_getchar();
	}

return len;
}

__attribute__((weak)) int _write(int file, const void *ptr, size_t len)
{
	(void) file;  // no files here
	const char *p = ptr;
	for (size_t DataIdx = 0; DataIdx < len; DataIdx++)
	{
		__io_putchar(*p++);
	}
	return len;
}

void *_sbrk(ptrdiff_t incr)
{
	// See https://gcc.gnu.org/onlinedocs/gcc/Local-Register-Variables.html
	register char * stack_ptr __asm__("sp");
	extern char end __asm__("end");
	static char *heap_end;
	char *prev_heap_end;

	if (heap_end == 0)
		heap_end = &end;

	prev_heap_end = heap_end;
	if (heap_end + incr > stack_ptr)
	{
		if (0) {
			_write(1, "Heap and stack collision\n", 25);
			abort();
		}
		errno = ENOMEM;
		return (caddr_t) -1;
	}

	heap_end += incr;

	return (caddr_t) prev_heap_end;
}

int _close(int file)
{
	(void) file;
	return -1;
}

int _fstat(int file, struct stat *st)
{
	(void) file;
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file)
{
	(void) file;
	return 1;
}

off_t _lseek(int file, off_t ptr, int dir)
{
	(void) file;  (void) ptr;  (void) dir;
	return 0;
}

int _open(const char *path, int flags, ...)
{
	/* Pretend like we always fail */
	(void) path;  (void) flags;
	return -1;
}

int _wait(int *status)
{
	(void) status;
	errno = ECHILD;
	return -1;
}

int _unlink(const char *name)
{
	(void) name;
	errno = ENOENT;
	return -1;
}

clock_t _times(struct tms *buf)
{
	(void) buf;
	return -1;
}

int _stat(const char *file, struct stat *st)
{
	(void) file;
	st->st_mode = S_IFCHR;
	return 0;
}

int _link(const char *old, const char *new)
{
	(void) old;  (void) new;
	errno = EMLINK;
	return -1;
}

int _fork(void)
{
	errno = EAGAIN;
	return -1;
}

int _execve(const char *name, char * const argv[], char * const env[])
{
	(void) name;  (void) argv;  (void) env;
	errno = ENOMEM;
	return -1;
}
