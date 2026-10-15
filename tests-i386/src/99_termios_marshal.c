/*
 * 99_termios_marshal — tcgetattr/tcsetattr with the i386 struct termios
 * (tcflag_t/speed_t are 4 bytes: 44 bytes in all, 72 on x86_64). Neither has
 * a generated bridge; the raw native call crashed Portal's
 * CTextConsoleUnix::Init in tcsetattr.
 *
 * On a pty slave: read the settings, check nothing was written past the
 * 44-byte struct, clear ECHO and set VMIN, write them back, read them again.
 *
 * Exit 42 = the change round-tripped. Kill switch M64_NO_TERMIOS_SHIM=1 (run
 * time): the native call on the i386 struct, a wrong exit or a fault.
 */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

int main(void) {
   struct { struct termios t; unsigned char pad[32]; } b;
   struct termios u;
   const char *name;
   int m = posix_openpt(O_RDWR | O_NOCTTY), fd, k;
   if (m < 0 || grantpt(m) != 0 || unlockpt(m) != 0 || !(name = ptsname(m))) exit(10);
   if ((fd = open(name, O_RDWR | O_NOCTTY)) < 0) exit(9);
   memset(&b, 0xab, sizeof b);
   if (tcgetattr(fd, &b.t) != 0) exit(11);
   for (k = 0; k < (int)sizeof b.pad; ++k) {
      if (b.pad[k] != 0xab) exit(12);         /* written past the i386 struct */
   }
   if (b.t.c_ispeed == 0xabababab || b.t.c_ospeed == 0xabababab) exit(13);
   b.t.c_lflag &= ~(tcflag_t)ECHO;
   b.t.c_cc[VMIN] = 7;
   if (tcsetattr(fd, TCSANOW, &b.t) != 0) exit(14);
   memset(&u, 0, sizeof u);
   if (tcgetattr(fd, &u) != 0) exit(15);
   if (u.c_cc[VMIN] != 7 || (u.c_lflag & ECHO)) exit(16);
   exit(42);
}
