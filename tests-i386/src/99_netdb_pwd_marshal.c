/* 99_netdb_pwd_marshal — libc's static-struct lookups come back in the i386
 * layout.
 *
 * gethostbyname/getservbyname/getprotobyname/getpwuid/getgrgid return a pointer
 * to libSystem's static struct. The bridge used to hand the native x86_64 struct
 * over as-is (8-byte pointers), so i386 code read the wrong fields: Portal 2's
 * NET_GetLocalAddress took h_addrtype (AF_INET = 2) for h_addr_list and
 * dereferenced 2 once the machine's own hostname resolved.
 *
 * Every answer here is fixed on any Mac: localhost = 127.0.0.1, http = tcp/80,
 * tcp = protocol 6, root's home is /var/root, group 0 is wheel.
 * ON exits 42. OFF: M64_NO_NETDB_MARSHAL=1 (run time) -> native layout -> a
 * wrong field (exit 1..5) or a fault.
 */
extern int  printf(const char *, ...);
extern void exit(int);
extern int  strcmp(const char *, const char *);

struct hostent  { char *h_name; char **h_aliases; int h_addrtype; int h_length; char **h_addr_list; };
struct servent  { char *s_name; char **s_aliases; int s_port; char *s_proto; };
struct protoent { char *p_name; char **p_aliases; int p_proto; };
struct passwd   { char *pw_name, *pw_passwd; unsigned pw_uid, pw_gid; long pw_change;
                  char *pw_class, *pw_gecos, *pw_dir, *pw_shell; long pw_expire; };
struct group    { char *gr_name, *gr_passwd; unsigned gr_gid; char **gr_mem; };
struct hostent  *gethostbyname(const char *);
struct servent  *getservbyname(const char *, const char *);
struct protoent *getprotobyname(const char *);
struct passwd   *getpwuid(unsigned);
struct group    *getgrgid(unsigned);

int main(void) {
   int step = 0;
   struct hostent *h = gethostbyname("localhost");
   if (!h || h->h_addrtype != 2 || h->h_length != 4 || !h->h_addr_list || !h->h_addr_list[0] ||
       (unsigned char)h->h_addr_list[0][0] != 127 || (unsigned char)h->h_addr_list[0][3] != 1) { step = 1; goto out; }
   struct servent *s = getservbyname("http", "tcp");
   if (!s || s->s_port != (80 << 8) /* network order */ || strcmp(s->s_proto, "tcp")) { step = 2; goto out; }
   struct protoent *p = getprotobyname("tcp");
   if (!p || p->p_proto != 6 || strcmp(p->p_name, "tcp")) { step = 3; goto out; }
   struct passwd *pw = getpwuid(0);
   if (!pw || pw->pw_uid != 0 || strcmp(pw->pw_name, "root") || strcmp(pw->pw_dir, "/var/root")) { step = 4; goto out; }
   struct group *g = getgrgid(0);
   if (!g || g->gr_gid != 0 || strcmp(g->gr_name, "wheel")) { step = 5; goto out; }
   step = 42;
out:
   printf("step=%d\n", step);
   exit(step);
}
