/* A wrapper which converts STDIN/STDOUT to a pseudoterminal (PTY)
 */

// Annoying gotcha _GNU_SOURCE needed by stdlib.h and unistd.h
#define _GNU_SOURCE
#include <sys/time.h> // For sleep
#include <unistd.h>   // For STDIN_FILENO, fork, sleep
#include <stdlib.h>   // For posix_openpt et al
#include <fcntl.h>    // For fcntl()
#include <signal.h>
#include <string.h>
#include <poll.h>     // For poll()
#include <errno.h>
#include <stdio.h>    // For heartbeat prints

#include <termios.h>

// TODO:
//  1. Is it safer to first check for existence of the binary before calling execv?
//     Or can execv safely handle nonsense?

// PARENT_HEARTBEAT enables a periodic print to stderr and to pty for debugging
//#define PARENT_HEARTBEAT
// CHILD_HEARTBEAT replaces execution of the target binary with an infinite loop
// of periodic prints to stdout (which should be redirected to the pty)
//#define CHILD_HEARTBEAT

static void _parentHandler(int c);
static void _childHandler(int c);
static int init(void);
static void parent_heartbeat(void);
static void child_heartbeat(void);
static void query_pty(int fd);
static int create_pty(void);
static void print_consts(void);

// I've tried many ways to wait for the paired side of the pty to be opened
// Only a wacky hack in wait_for_open4() has worked on my Ubuntu 22.04 machine
static int wait_for_open(int fd, int timeout_ms);
static int wait_for_open2(char *name);
static int wait_for_open3(char *name);
static int wait_for_open4(int fd);

#define MAX_ARGS        (10)
char devName[50];
char bname[100];
static int pty;
static int toExit = 0;
char *bargv[MAX_ARGS];
const char usage[] = \
  "USAGE: ptywrap myExecutable\r\n" \
  "Run 'myExecutable' and wrap its stdin/stdout interface to function as a ptyinal (PTY)\r\n";

static int wait_for_open4(int fd) {
  int rval;
  struct termios termios_s;
  while (!toExit) {
    errno = 0;
    if (tcgetattr(fd, &termios_s) < 0) {
      perror("tcgetattr error: ");
      return -1;
    };
    // HACK ALERT! I'm using the CLOCAL flag as a proxy for "a reader has opened the other side"
    // since no standard method exists for getting that information (and suggestions from stack
    // overflow don't seem to work).
    if (termios_s.c_cflag & CLOCAL) {
      // Listener present
      break;
    }
  }
  return 0;
}

static int wait_for_open3(char *name) {
  // Write a newline, then wait for it to be read
  //int fd = open(name, O_RDWR);
  int fd = pty;
  write(fd, "\r\n", 2);
  errno = 0;
  if (tcdrain(fd) < 0) {;
    perror("tcdrain error: ");
  }
  printf("Opened\r\n");
  //close(fd);
  return 0;
}

static int wait_for_open2(char *name) {
  int rval;
  while (!toExit) {
    errno = 0;
    rval = open(name, O_RDWR);
    if (rval < 0) {
      //if (errno == ) {
      perror("Errno = ");
      if (1) {
        rval = 0; // Call this success
        break;
      }
    }
    close(rval);
    sleep(1);
  }
  return rval;
}

static int wait_for_open(int fd, int timeout_ms) {
  struct pollfd fds = {.fd=fd, .events = POLLHUP, .revents=0};
  int rval;
  while (!toExit) {
    rval = poll(&fds, 1, timeout_ms);
    if (!(fds.revents & POLLHUP)) {
      // Break on any revents that aren't hangup
      break;
    }
  }
  if (toExit) {
    return -1;
  }
  if (rval <= 0) {
    // Timeout or error
    if (rval == 0) {
      fprintf(stderr, "wait_for_open Timeout\r\n");
    } else {
      fprintf(stderr, "wait_for_open Error: rval = %d\r\n", rval);
    }
    return -1;
  }
  if (fds.revents & POLLPRI) {
    fprintf(stderr, "POLLPRI\r\n");
    return 1;
  } else {
    fprintf(stderr, "revents = 0x%x = %s\r\n", fds.revents,
            fds.revents & POLLHUP ? "POLLHUP" :
            fds.revents & POLLERR ? "POLLERR" :
            fds.revents & POLLNVAL ? "POLLNVAL" :
            "unknown");
  }
  return 0;
}

static int init(void) {
  int rval;
  toExit = 0;
  // Create PTY
  create_pty();
  // TODO - Figure out this waiting scheme
  if (1) {
    printf("Waiting for device %s\r\n", devName);
    if (0) {
      //return wait_for_open2(devName);
      return wait_for_open3(devName);
    } else {
      // Weird hack to set the POLLHUP bit on pty
      //close(open(devName, O_RDWR | O_NOCTTY));
      //return wait_for_open(pty, 10000);  // 10s timeout
      return wait_for_open4(pty);
    }
  } else {
    printf("Open device %s\r\n", devName);
    return 0;
  }
};

static void shutdown(void) {
  close(pty);
  return;
}

static void child_heartbeat(void) {
  static int _heartbeat = 0;
  if ((++_heartbeat % 500000000) == 0) {
    fprintf(stderr, "    ptywrap child heartbeat\r\n");
    printf("Child Heartbeat\r\n");
  }
  return;
}

static void parent_heartbeat(void) {
#ifdef PARENT_HEARTBEAT
  static int _heartbeat = 0;
  if ((++_heartbeat % 1000) == 0) {
    fprintf(stderr, "    ptywrap parent heartbeat\r\n");
    write(pty, "Parent Heartbeat\n", 10);
  }
#endif
  return;
}

int main(int argc, char *argv[]) {
  // Collect argv[1:] in a list of null-terminated strings
  int n;
  for (n = 1; n < argc; n++) {
    if (n == MAX_ARGS - 1) {
      break;
    }
    if (!argv[n]) {
      //printf("%d: Null pointer\r\n", n);
      break;
    } else {
      //printf("%d: %s\r\n", n, argv[n]);
      bargv[n-1] = argv[n];
    }
  }
  // The list MUST be terminated by a NULL pointer
  bargv[n] = NULL;

  // Safely copy the binary name
  if (argc > 1) {
    strncpy(bname, argv[1], sizeof(bname));
  } else {
    printf("%s", usage);
    return 1;
  }

  if (init() < 0) {
    shutdown();
    return 1;
  }

  fprintf(stderr, "Far end device open. Executing binary.\r\n");
  // Fork a child process
  pid_t pid = fork();
  // The value of 'pid' will be 0 in the child fork and will be some positive number
  // (the pid of the child process) in the parent fork. pid will be < 0 on error.
  if (pid < 0) {
    fprintf(stderr, "Forking error. Aborting\r\n");
  } else if (pid == 0) {
    // ============================ Child process ============================
    // Replace own STDOUT with pty
    if (dup2(pty, STDOUT_FILENO) < 0) {
      fprintf(stderr, "Failed to duplicate stdout\r\n");
      perror("Errno: ");
      toExit = 1;
    }
    // Replace own STDIN with pty
    if (dup2(pty, STDIN_FILENO) < 0) {
      fprintf(stderr, "Failed to duplicate stdin\r\n");
      perror("Errno: ");
      toExit = 1;
    }
    // Optional changes to printf buffering.  May not do anything.
    //setlinebuf(stdout);
    //setbuf(stdout, NULL);
    struct sigaction act = {.sa_handler=_childHandler};
    sigaction(SIGINT, &act, NULL);
    // Run the binary
#ifdef CHILD_HEARTBEAT
    while (!toExit) {
      child_heartbeat();
    }
#else
    execv(bname, bargv);
#endif
    // Will only arrive here if execv() encounters an error launching the binary
    fprintf(stderr, "execv error!\r\n");
    _exit(0);
  } else {
    // =========================== Parent process ============================
    // Look for interrupt signal only in parent process
    struct sigaction act = {.sa_handler=_parentHandler};
    sigaction(SIGINT, &act, NULL);
    // Also look for signal from the child process
    sigaction(SIGCHLD, &act, NULL);
    while (!toExit) {
      /*
      query_pty(pty);
      sleep(1);
      */
    }
    // Terminate the child process
    kill(pid, SIGKILL);
  }
  shutdown();
  fprintf(stderr, "Done\r\n");
  // If we get here without setting toExit=1, we had an error
  return !toExit;
}

static void _parentHandler(int c) {
  fprintf(stderr, "Received signal %d. Exiting...\r\n", c);
  toExit = 1;
  return;
}

static void _childHandler(int c) {
  toExit = 1;
  return;
}

static int create_pty(void) {
  char* dname;
  int rval;
  pty = posix_openpt(O_RDWR | O_NOCTTY);
  // Get the pty's name
  dname = ptsname(pty);
  // Save it for later
  strncpy(devName, dname, sizeof(devName));
  // Null-term just in case
  devName[49] = '\0';
  // Do this silly mandatory nonsense
  rval = grantpt(pty);
  rval = unlockpt(pty);
  // Change settings
  struct termios termios_s;
  errno = 0;
  if (tcgetattr(pty, &termios_s) < 0) {
    perror("tcgetattr error: ");
    return -1;
  };
  // Disable echo
  termios_s.c_lflag &= ~(ECHO);
  if (tcsetattr(pty, TCSANOW, &termios_s) < 0) {
    perror("tcsetattr error: ");
    return -1;
  }
  return 0;
}

// =========== Experimentally determining how to tell when the paired side pts is open ============

static void query_pty(int fd) {
  struct termios termios_s;
  errno = 0;
  if (tcgetattr(fd, &termios_s) < 0) {
    perror("tcgetattr error: ");
    return;
  };
  //printf("Echo %s\r\n", termios_s.c_lflag & ECHO ? "enabled" : "disabled");
  fprintf(stderr, "termios_s.c_iflag = 0x%x\r\n", termios_s.c_iflag);
  fprintf(stderr, "termios_s.c_oflag = 0x%x\r\n", termios_s.c_oflag);
  fprintf(stderr, "termios_s.c_cflag = 0x%x\r\n", termios_s.c_cflag);
  fprintf(stderr, "termios_s.c_lflag = 0x%x\r\n", termios_s.c_lflag);
  fprintf(stderr, "\r\n");
  return;
}

#define X(d, s) fprintf(stderr, "%s is 0x%x\r\n", s, d);
#define FOR_EACH_IFLAG() do { \
  X(IGNBRK, "IGNBRK") \
  X(BRKINT, "BRKINT") \
  X(IGNPAR, "IGNPAR") \
  X(PARMRK, "PARMRK") \
  X(INPCK, "INPCK") \
  X(ISTRIP, "ISTRIP") \
  X(INLCR, "INLCR") \
  X(IGNCR, "IGNCR") \
  X(ICRNL, "ICRNL") \
  X(IUCLC, "IUCLC") \
  X(IXON, "IXON") \
  X(IXANY, "IXANY") \
  X(IXOFF, "IXOFF") \
  X(IMAXBEL, "IMAXBEL") \
  X(IUTF8, "IUTF8") \
} while (0)

#define FOR_EACH_OFLAG() do { \
  X(OPOST, "OPOST") \
  X(OLCUC, "OLCUC") \
  X(ONLCR, "ONLCR") \
  X(OCRNL, "OCRNL") \
  X(ONOCR, "ONOCR") \
  X(ONLRET, "ONLRET") \
  X(OFILL, "OFILL") \
  X(OFDEL, "OFDEL") \
  X(NLDLY, "NLDLY") \
  X(CRDLY, "CRDLY") \
  X(TABDLY, "TABDLY") \
  X(BSDLY, "BSDLY") \
  X(VTDLY, "VTDLY") \
  X(FFDLY, "FFDLY") \
} while (0)

#define FOR_EACH_CFLAG() do { \
  X(CBAUD, "CBAUD") \
  X(CBAUDEX, "CBAUDEX") \
  X(CSIZE, "CSIZE") \
  X(CSTOPB, "CSTOPB") \
  X(CREAD, "CREAD") \
  X(PARENB, "PARENB") \
  X(PARODD, "PARODD") \
  X(HUPCL, "HUPCL") \
  X(CLOCAL, "CLOCAL") \
  X(CIBAUD, "CIBAUD") \
  X(CMSPAR, "CMSPAR") \
  X(CRTSCTS, "CRTSCTS") \
} while (0)

#define FOR_EACH_LFLAG() do { \
  X(ISIG, "ISIG") \
  X(ICANON, "ICANON") \
  X(XCASE, "XCASE") \
  X(ECHO, "ECHO") \
  X(ECHOE, "ECHOE") \
  X(ECHOK, "ECHOK") \
  X(ECHONL, "ECHONL") \
  X(ECHOCTL, "ECHOCTL") \
  X(ECHOPRT, "ECHOPRT") \
  X(ECHOKE, "ECHOKE") \
  X(FLUSHO, "FLUSHO") \
  X(NOFLSH, "NOFLSH") \
  X(TOSTOP, "TOSTOP") \
  X(PENDIN, "PENDIN") \
  X(IEXTEN, "IEXTEN") \
} while (0)

static void print_consts(void) {
  fprintf(stderr, "\r\niflags\r\n"); \
  FOR_EACH_IFLAG();
  fprintf(stderr, "\r\noflags\r\n"); \
  FOR_EACH_OFLAG();
  fprintf(stderr, "\r\ncflags\r\n"); \
  FOR_EACH_CFLAG();
  fprintf(stderr, "\r\nlflags\r\n"); \
  FOR_EACH_LFLAG();
  return;
};

/*
termios_s.c_iflag = 0x500
termios_s.c_oflag = 0x5
termios_s.c_cflag = 0xbf
termios_s.c_lflag = 0x8a3b

termios_s.c_iflag = 0x0
termios_s.c_oflag = 0x0
termios_s.c_cflag = 0x8bd
termios_s.c_lflag = 0x0

iflag 0x500 -> 0:     bits 8 and 10 deasserted (0x100 & 0x400)
  ICRNL is 0x100
    Translate carriage return to newline on input (unless IGNCR is set).
  IXON is 0x400
    Enable XON/XOFF flow control on output.

oflag 0x5 -> 0:       bits 0 and 2 deasserted  (0x1 & 0x4)
  OPOST is 0x1
    Enable implementation-defined output processing.
  ONLCR is 0x4
    (XSI) Map NL to CR-NL on output.

cflag 0xbf -> 0x8bd:  bit 0 deasserted (0x1), bit 11 asserted (0x800)
  CBAUD is 0x100f
    (not in POSIX) Baud speed mask (4+1 bits).  [requires _BSD_SOURCE or _SVID_SOURCE]
  CLOCAL is 0x800
    Ignore modem control lines.

lflag 0x8a3b -> 0x0:  bits 0, 1, 3, 4, 5, 9, 11, 15 deasserted (0x1, 0x2, 0x8, 0x10, 0x20, 0x200, 0x800, 0x8000)
  ISIG is 0x1
    When any of the characters INTR, QUIT, SUSP, or DSUSP are received, generate the corresponding signal.
  ICANON is 0x2
    Enable canonical mode (described below).
  ECHO is 0x8
    Echo input characters.
  ECHOE is 0x10
    If ICANON is also set, the ERASE character erases the preceding input character, and WERASE erases the preceding word.
  ECHOK is 0x20
    If ICANON is also set, the KILL character erases the current line.
  ECHOCTL is 0x200
    (not  in POSIX) If ECHO is also set, terminal special characters other than TAB, NL, START, and STOP are echoed as ^X,
    where X is the character with ASCII code 0x40 greater than the special character.  For example, character 0x08 (BS)
    is echoed as ^H. [requires _BSD_SOURCE or _SVID_SOURCE]
  ECHOKE is 0x800
    (not in POSIX) If ICANON is also set, KILL is echoed by erasing each character on the line, as specified by ECHOE and
    ECHOPRT.  [requires  _BSD_SOURCE  or _SVID_SOURCE]
  IEXTEN is 0x8000
    Enable implementation-defined input processing.  This flag, as well as ICANON must be enabled for the special characters
    EOL2, LNEXT, REPRINT, WERASE to be interpreted, and for the IUCLC flag to be effective.
 */
